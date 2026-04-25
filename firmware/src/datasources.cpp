/**
 * Datasources — registry of HTTP-poll, push, and internal value sources.
 *
 * Phase 1 scope: push + internal:clock are real. HTTP polling is stubbed
 * (Phase 2 — needs jsonpath-lite or a small JSONPath walker).
 *
 * The "value tree" is a JsonDocument keyed by datasource id at the top level;
 * each datasource owns a sub-object whose fields are read via dotted paths
 * like "claude.session_pct" or "clock.hhmm".
 */
#include "datasources.h"

#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include <set>
#include <time.h>
#include <vector>

namespace cydstudio::datasources {

namespace {

struct PushSource {
  String                 id;
  uint32_t               stale_after_ms = 0;
  uint32_t               last_push_ms   = 0;
  std::set<String>       allowed_fields;   // empty = all allowed
};

struct ClockSource {
  String   id;
  String   tz;
  uint32_t last_tick_ms = 0;
};

std::vector<PushSource>  push_sources;
std::vector<ClockSource> clock_sources;
JsonDocument             value_tree;          // root: { source_id: { field: value, ... } }
std::vector<ChangeCb>    change_callbacks;

void notify(const String& path) {
  for (auto& cb : change_callbacks) cb(path);
}

void persist_source(const String& id) {
  auto obj = value_tree[id].as<JsonObjectConst>();
  if (obj.isNull()) return;
  String s; serializeJson(obj, s);
  Preferences prefs; prefs.begin("cydvals", false);
  String key = "ds_" + id; key.remove(15);  // NVS key cap
  prefs.putString(key.c_str(), s);
  prefs.end();
}

void restore_source(const String& id) {
  Preferences prefs; prefs.begin("cydvals", true);
  String key = "ds_" + id; key.remove(15);
  String s = prefs.getString(key.c_str(), "");
  prefs.end();
  if (s.length() == 0) return;
  JsonDocument tmp;
  if (deserializeJson(tmp, s)) return;
  auto dst = value_tree[id].to<JsonObject>();
  for (JsonPairConst kv : tmp.as<JsonObjectConst>())
    dst[kv.key()] = kv.value();
  Serial.printf("[ds] restored %s from NVS\n", id.c_str());
}

// --- __page.pct (carousel progress) -----------------------------------------
static uint32_t    page_start_ms    = 0;
static uint32_t    page_dur_ms      = 0;
static lv_timer_t* page_pct_timer   = nullptr;

static void page_pct_timer_cb(lv_timer_t*) {
  if (page_dur_ms == 0) return;
  uint32_t elapsed = millis() - page_start_ms;
  int pct = (int)min(100UL, (unsigned long)elapsed * 100UL / page_dur_ms);
  value_tree["__page"]["pct"] = pct;
  notify("__page.pct");
}
// -----------------------------------------------------------------------------

void apply_clock(ClockSource& c) {
  time_t now = time(nullptr);
  struct tm tm_local;
  localtime_r(&now, &tm_local);
  char hhmm[8];
  strftime(hhmm, sizeof(hhmm), "%H:%M", &tm_local);
  char hhmmss[12];
  strftime(hhmmss, sizeof(hhmmss), "%H:%M:%S", &tm_local);
  // Direct field assignment — DON'T use .to<JsonObject>() because that wipes
  // every existing key in this sub-object on each tick (the bug that caused
  // bound widgets to briefly read `null` between wipe and re-set).
  String prev = value_tree[c.id]["hhmm"].as<String>();
  value_tree[c.id]["hhmm"]   = String(hhmm);
  value_tree[c.id]["hhmmss"] = String(hhmmss);
  value_tree[c.id]["epoch"]  = (uint32_t)now;
  if (String(hhmm) != prev) notify(c.id + ".hhmm");
}

}  // namespace

void configure(JsonArrayConst sources) {
  push_sources.clear();
  clock_sources.clear();
  value_tree.clear();
  value_tree["__page"]["pct"] = 0;

  for (JsonObjectConst s : sources) {
    const char* type = s["type"];
    const char* id   = s["id"];
    if (!type || !id) continue;

    if (strcmp(type, "push") == 0) {
      PushSource p;
      p.id              = id;
      p.stale_after_ms  = (uint32_t)(s["stale_after_sec"] | 0) * 1000U;
      if (s["fields"].is<JsonArrayConst>()) {
        for (JsonVariantConst f : s["fields"].as<JsonArrayConst>())
          p.allowed_fields.insert(f.as<String>());
      }
      push_sources.push_back(std::move(p));
      value_tree[id].to<JsonObject>();
      restore_source(String(id));
    } else if (strcmp(type, "internal") == 0 && strcmp(s["source"] | "", "clock") == 0) {
      ClockSource c;
      c.id = id;
      c.tz = s["tz"].as<String>();
      if (c.tz.length()) {
        setenv("TZ", c.tz.c_str(), 1);
        tzset();
      }
      clock_sources.push_back(std::move(c));
      value_tree[id].to<JsonObject>();
    } else {
      // http and other internal sources: TODO Phase 2
      Serial.printf("[ds] skipping unsupported source type=%s\n", type);
    }
  }
}

bool resolve(const String& path, JsonVariant& out) {
  int dot = path.indexOf('.');
  if (dot <= 0) return false;
  String src = path.substring(0, dot);
  String field = path.substring(dot + 1);
  JsonVariantConst node = value_tree[src][field];
  if (node.isNull()) return false;
  out.set(node);
  return true;
}

void tick() {
  uint32_t now_ms = millis();

  for (auto& c : clock_sources) {
    if (now_ms - c.last_tick_ms >= 1000U) {
      apply_clock(c);
      c.last_tick_ms = now_ms;
    }
  }

  // Stale-mark push sources whose last push is older than stale_after_ms.
  for (auto& p : push_sources) {
    if (p.stale_after_ms == 0 || p.last_push_ms == 0) continue;
    if (now_ms - p.last_push_ms > p.stale_after_ms) {
      auto obj = value_tree[p.id].as<JsonObject>();
      if (!obj["__stale"].as<bool>()) {
        obj["__stale"] = true;
        notify(p.id + ".__stale");
      }
    }
  }
}

String accept_push(const String& source_id, const JsonDocument& body) {
  PushSource* src = nullptr;
  for (auto& p : push_sources) if (p.id == source_id) { src = &p; break; }
  if (!src) return "unknown push source: " + source_id;

  if (!body.is<JsonObjectConst>()) return "body must be a JSON object";

  auto obj = value_tree[source_id].to<JsonObject>();
  obj["__stale"] = false;

  for (JsonPairConst kv : body.as<JsonObjectConst>()) {
    String key = kv.key().c_str();
    if (!src->allowed_fields.empty() && !src->allowed_fields.count(key))
      return "field not allowed: " + key;
    obj[key] = kv.value();
    notify(source_id + "." + key);
  }
  src->last_push_ms = millis();
  persist_source(source_id);
  return "";
}

void on_change(ChangeCb cb) {
  change_callbacks.push_back(std::move(cb));
}

void page_started(uint32_t duration_ms) {
  if (page_pct_timer) { lv_timer_del(page_pct_timer); page_pct_timer = nullptr; }
  page_start_ms = millis();
  page_dur_ms   = duration_ms;
  value_tree["__page"]["pct"] = 0;
  notify("__page.pct");
  if (duration_ms > 0)
    page_pct_timer = lv_timer_create(page_pct_timer_cb, 500, nullptr);
}

}  // namespace cydstudio::datasources
