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

void apply_clock(ClockSource& c) {
  time_t now = time(nullptr);
  struct tm tm_local;
  localtime_r(&now, &tm_local);
  char hhmm[8];
  strftime(hhmm, sizeof(hhmm), "%H:%M", &tm_local);
  char hhmmss[12];
  strftime(hhmmss, sizeof(hhmmss), "%H:%M:%S", &tm_local);
  auto obj = value_tree[c.id].to<JsonObject>();
  if (String(hhmm) != obj["hhmm"].as<String>()) {
    obj["hhmm"] = String(hhmm);
    notify(c.id + ".hhmm");
  }
  obj["hhmmss"] = String(hhmmss);
  obj["epoch"]  = (uint32_t)now;
}

}  // namespace

void configure(JsonArrayConst sources) {
  push_sources.clear();
  clock_sources.clear();
  value_tree.clear();

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
  return "";
}

void on_change(ChangeCb cb) {
  change_callbacks.push_back(std::move(cb));
}

}  // namespace cydstudio::datasources
