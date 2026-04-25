/**
 * Renderer — translates a layout JSON into LVGL widgets and keeps them in
 * sync with datasource changes.
 *
 * Schema 0.2 adds multi-page layouts: the firmware tracks all defined pages
 * and continuously re-evaluates each page's `show_when` expression. The first
 * page whose expression is truthy becomes the active screen; the rest stay
 * dormant. Single-page (schema 0.1) layouts still work — they're treated as
 * one implicit page named "default" with an empty (always-true) show_when.
 *
 * Widgets supported: label (incl. blink), bar, arc, image (builtin:*), line.
 */
#include "renderer.h"

#include <Preferences.h>
#include <lvgl.h>
#include <vector>

#include "builtin_images.h"
#include "datasources.h"
#include "display.h"
#include "format.h"
#include "settings.h"

namespace cydstudio::renderer {

namespace {

struct BoundWidget {
  lv_obj_t*    obj         = nullptr;
  lv_timer_t*  blink_timer = nullptr;   // non-null only for blinking labels
  String       type;
  String       binding;
  String       fmt;
  String       static_text;             // for label fallback when binding missing
  String       hidden_when;
  float        min = 0, max = 100;
};

struct PageDef {
  String   id;
  String   show_when;
  uint32_t auto_advance_ms = 0;     // 0 = no carousel
  String   transition;               // "none","slide_left","slide_right","slide_up","slide_down","fade"
  int      doc_index = -1;
};

lv_obj_t*                current_screen = nullptr;
std::vector<BoundWidget> bound_widgets;
std::vector<PageDef>     pages;
int                      current_page_idx = -1;
JsonDocument             active_doc;
bool                     change_handler_installed = false;
uint32_t                 g_restart_pending_at_ms = 0;
lv_timer_t*              g_carousel_timer = nullptr;

lv_color_t hex(const char* s, lv_color_t fallback) {
  if (!s || s[0] != '#' || strlen(s) != 7) return fallback;
  return lv_color_hex((uint32_t)strtoul(s + 1, nullptr, 16));
}

const lv_font_t* font_for(int size) {
  switch (size) {
    case 12: return &lv_font_montserrat_12;
    case 16: return &lv_font_montserrat_16;
    case 20: return &lv_font_montserrat_20;
    case 24: return &lv_font_montserrat_24;
    default: return &lv_font_montserrat_14;
  }
}

bool eval_expr(const String& expr) {
  if (expr.length() == 0) return true;
  // Supported: "<binding>", "!<binding>" (truthiness check on resolved value).
  String e = expr; e.trim();
  bool negate = false;
  if (e.startsWith("!")) { negate = true; e = e.substring(1); e.trim(); }
  JsonDocument tmp; JsonVariant v = tmp.to<JsonVariant>();
  bool found = datasources::resolve(e, v);
  bool truthy = false;
  if (found) {
    if (v.is<bool>())             truthy = v.as<bool>();
    else if (v.is<int>())         truthy = v.as<int>() != 0;
    else if (v.is<float>())       truthy = v.as<float>() != 0.0f;
    else if (v.is<const char*>()) truthy = v.as<String>().length() > 0;
    else                          truthy = !v.isNull();
  }
  return negate ? !truthy : truthy;
}

void update_widget_value(BoundWidget& w) {
  if (w.hidden_when.length()) {
    bool hide = eval_expr(w.hidden_when);
    if (hide) lv_obj_add_flag(w.obj, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_clear_flag(w.obj, LV_OBJ_FLAG_HIDDEN);
  }
  if (w.binding.length() == 0) return;
  JsonDocument tmp; JsonVariant v = tmp.to<JsonVariant>();
  bool found = datasources::resolve(w.binding, v);

  if (w.type == "label") {
    String txt;
    if (found) txt = w.fmt.length() ? format::apply(w.fmt, v) : v.as<String>();
    else       txt = w.static_text;
    // ArduinoJson stringifies a JSON null as the literal "null" — never the
    // intent for a UI label, fall back to static text (or empty).
    if (txt == "null") txt = w.static_text;
    lv_label_set_text(w.obj, txt.c_str());
  } else if (w.type == "bar") {
    int val = found ? v.as<int>() : (int)w.min;
    lv_bar_set_value(w.obj, val, LV_ANIM_OFF);
  } else if (w.type == "arc") {
    int val = found ? v.as<int>() : (int)w.min;
    lv_arc_set_value(w.obj, val);
  }
}

static void blink_timer_cb(lv_timer_t* t) {
  auto* obj = static_cast<lv_obj_t*>(t->user_data);
  if (!obj) return;
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else                                          lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void clear_bound_widgets() {
  // Tear down any blink timers before the LVGL objects they target get freed
  // by the screen delete; otherwise the timer fires on a dangling pointer.
  for (auto& w : bound_widgets) {
    if (w.blink_timer) { lv_timer_del(w.blink_timer); w.blink_timer = nullptr; }
  }
  bound_widgets.clear();
}

void on_datasource_change(const String& path);  // fwd decl

void build_label(JsonObjectConst spec, lv_obj_t* parent) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_text_font(lbl, font_for(spec["font"] | 14), 0);
  lv_obj_set_style_text_color(lbl, hex(spec["color"], lv_color_white()), 0);
  const char* align = spec["align"] | "left";
  int x = spec["x"] | 0;
  int y = spec["y"] | 0;
  if (strcmp(align, "right") == 0) {
    int w = spec["w"] | x;
    lv_obj_set_width(lbl, w);
    lv_obj_set_pos(lbl, x - w, y);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_RIGHT, 0);
  } else if (strcmp(align, "center") == 0) {
    int w = spec["w"] | (parent->coords.x2 - parent->coords.x1 + 1);
    lv_obj_set_width(lbl, w);
    lv_obj_set_pos(lbl, x - w / 2, y);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  } else {
    if (spec["w"].is<int>()) lv_obj_set_width(lbl, spec["w"].as<int>());
    lv_obj_set_pos(lbl, x, y);
  }

  // ArduinoJson v7: `.as<String>()` on a missing key returns the literal
  // "null" — use the `| ""` fallback instead so missing fields stay empty.
  BoundWidget w;
  w.obj         = lbl;
  w.type        = "label";
  w.binding     = String(spec["binding"]     | "");
  w.fmt         = String(spec["format"]      | "");
  w.static_text = String(spec["text"]        | "");
  w.hidden_when = String(spec["hidden_when"] | "");
  if (w.binding.length() == 0) lv_label_set_text(lbl, w.static_text.c_str());
  if (spec["blink"] | false) {
    int interval = spec["blink_interval_ms"] | 530;
    w.blink_timer = lv_timer_create(blink_timer_cb, interval, lbl);
  }
  bound_widgets.push_back(w);
  update_widget_value(bound_widgets.back());
}

void build_bar(JsonObjectConst spec, lv_obj_t* parent) {
  lv_obj_t* bar = lv_bar_create(parent);
  lv_obj_set_pos(bar, spec["x"] | 0, spec["y"] | 0);
  lv_obj_set_size(bar, spec["w"] | 100, spec["h"] | 8);
  float mn = spec["min"] | 0.0f, mx = spec["max"] | 100.0f;
  lv_bar_set_range(bar, (int)mn, (int)mx);
  lv_obj_set_style_bg_color(bar, hex(spec["bg"], lv_color_hex(0x2A2A2A)), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, hex(spec["color"], lv_color_hex(0xD95A12)), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  if (!(spec["rounded"] | true)) {
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
  }
  // Bar without a binding: fill fully so the indicator color shows solid.
  if ((spec["binding"] | "") == "") lv_bar_set_value(bar, (int)mx, LV_ANIM_OFF);

  BoundWidget w;
  w.obj         = bar;
  w.type        = "bar";
  w.binding     = String(spec["binding"]     | "");
  w.hidden_when = String(spec["hidden_when"] | "");
  w.min = mn; w.max = mx;
  if (spec["blink"] | false) {
    int interval = spec["blink_interval_ms"] | 530;
    w.blink_timer = lv_timer_create(blink_timer_cb, interval, bar);
  }
  bound_widgets.push_back(w);
  update_widget_value(bound_widgets.back());
}

void build_arc(JsonObjectConst spec, lv_obj_t* parent) {
  lv_obj_t* arc = lv_arc_create(parent);
  int size = spec["size"] | 80;
  lv_obj_set_pos(arc, spec["x"] | 0, spec["y"] | 0);
  lv_obj_set_size(arc, size, size);
  float mn = spec["min"] | 0.0f, mx = spec["max"] | 100.0f;
  lv_arc_set_range(arc, (int)mn, (int)mx);
  lv_arc_set_bg_angles(arc, spec["start_angle"] | 135, spec["end_angle"] | 45);
  lv_obj_set_style_arc_width(arc, spec["thickness"] | 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, spec["thickness"] | 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, hex(spec["bg"], lv_color_hex(0x2A2A2A)), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, hex(spec["color"], lv_color_hex(0xD95A12)), LV_PART_INDICATOR);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);

  BoundWidget w;
  w.obj         = arc;
  w.type        = "arc";
  w.binding     = String(spec["binding"]     | "");
  w.hidden_when = String(spec["hidden_when"] | "");
  w.min = mn; w.max = mx;
  bound_widgets.push_back(w);
  update_widget_value(bound_widgets.back());
}

void build_line(JsonObjectConst spec, lv_obj_t* parent) {
  static lv_point_t pts_storage[64][2];
  static int pts_idx = 0;
  if (pts_idx >= 64) return;
  pts_storage[pts_idx][0] = { (lv_coord_t)(spec["x1"] | 0), (lv_coord_t)(spec["y1"] | 0) };
  pts_storage[pts_idx][1] = { (lv_coord_t)(spec["x2"] | 0), (lv_coord_t)(spec["y2"] | 0) };
  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, pts_storage[pts_idx], 2);
  pts_idx++;
  lv_obj_set_style_line_color(line, hex(spec["color"], lv_color_hex(0x444444)), 0);
  lv_obj_set_style_line_width(line, spec["thickness"] | 1, 0);
}

void build_image(JsonObjectConst spec, lv_obj_t* parent) {
  const char* src = spec["src"] | "";
  if (strncmp(src, "builtin:", 8) != 0) {
    Serial.printf("[render] image src not yet supported: %s\n", src);
    return;
  }
  const lv_img_dsc_t* img = lookup_builtin_image(src + 8);
  if (!img) { Serial.printf("[render] no builtin image '%s'\n", src + 8); return; }
  lv_obj_t* obj = lv_img_create(parent);
  lv_img_set_src(obj, img);
  lv_obj_set_pos(obj, spec["x"] | 0, spec["y"] | 0);
}

void build_widget(JsonObjectConst w, lv_obj_t* parent) {
  const char* type = w["type"] | "";
  if      (strcmp(type, "label") == 0) build_label(w, parent);
  else if (strcmp(type, "bar")   == 0) build_bar(w, parent);
  else if (strcmp(type, "arc")   == 0) build_arc(w, parent);
  else if (strcmp(type, "line")  == 0) build_line(w, parent);
  else if (strcmp(type, "image") == 0) build_image(w, parent);
  else Serial.printf("[render] unknown widget type: %s\n", type);
}

JsonObjectConst page_object_at(int idx) {
  auto pages_arr = active_doc["pages"].as<JsonArrayConst>();
  if (!pages_arr.isNull()) {
    int i = 0;
    for (JsonObjectConst p : pages_arr) {
      if (i == idx) return p;
      ++i;
    }
    return JsonObjectConst();
  }
  // Single-page (schema 0.1) fallback: synthesize from the top-level document.
  // We just return the root as a "page-like" object — bg comes from ["page"]["bg"]
  // and widgets from ["widgets"]. render_page handles both shapes.
  return active_doc.as<JsonObjectConst>();
}

lv_scr_load_anim_t parse_transition(const String& name) {
  if (name == "slide_left")  return LV_SCR_LOAD_ANIM_MOVE_LEFT;
  if (name == "slide_right") return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
  if (name == "slide_up")    return LV_SCR_LOAD_ANIM_MOVE_TOP;
  if (name == "slide_down")  return LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
  if (name == "fade")        return LV_SCR_LOAD_ANIM_FADE_IN;
  return LV_SCR_LOAD_ANIM_NONE;
}

void schedule_carousel_for(int idx);  // fwd

void render_page(int idx) {
  if (idx < 0 || idx >= (int)pages.size()) return;
  if (idx == current_page_idx && current_screen != nullptr) return;

  JsonObjectConst page = page_object_at(idx);

  lv_obj_t* prev_screen = current_screen;
  clear_bound_widgets();

  current_screen = lv_obj_create(nullptr);
  lv_obj_remove_style_all(current_screen);
  const char* bg = "#111111";
  if (page["bg"].is<const char*>())                       bg = page["bg"].as<const char*>();
  else if (active_doc["page"]["bg"].is<const char*>())    bg = active_doc["page"]["bg"].as<const char*>();
  lv_obj_set_style_bg_color(current_screen, hex(bg, lv_color_hex(0x111111)), 0);
  lv_obj_set_style_bg_opa(current_screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(current_screen, LV_OBJ_FLAG_SCROLLABLE);

  // Use animated load if the destination page declares a transition (and we
  // have a previous screen to slide AWAY from). Animated loads automatically
  // delete the previous screen for us — never call lv_obj_del on it after.
  lv_scr_load_anim_t anim = parse_transition(pages[idx].transition);
  if (prev_screen != nullptr && anim != LV_SCR_LOAD_ANIM_NONE) {
    lv_scr_load_anim(current_screen, anim, /*time*/200, /*delay*/0, /*auto_del*/true);
  } else {
    lv_scr_load(current_screen);
    if (prev_screen) lv_obj_del(prev_screen);
  }

  JsonArrayConst widgets = page["widgets"].as<JsonArrayConst>();
  if (widgets.isNull()) widgets = active_doc["widgets"].as<JsonArrayConst>();
  for (JsonObjectConst w : widgets) build_widget(w, current_screen);

  current_page_idx = idx;
  Serial.printf("[render] page → %s (idx %d)\n", pages[idx].id.c_str(), idx);

  // Set up auto-advance for the new page if it asked for one.
  schedule_carousel_for(idx);
  datasources::page_started(pages[idx].auto_advance_ms);
}

// Run on the next LVGL event-loop tick — must NOT happen synchronously inside
// the timer callback because render_page reschedules the carousel, which
// cancels (lv_timer_del) the timer that's currently firing → double free.
void do_advance(void* /*user_data*/) {
  if (pages.empty()) return;
  int n = pages.size();
  for (int step = 1; step <= n; ++step) {
    int candidate = (current_page_idx + step) % n;
    if (pages[candidate].show_when.length() == 0) {
      render_page(candidate);
      return;
    }
  }
}

void carousel_advance_cb(lv_timer_t* /*t*/) {
  lv_async_call(do_advance, nullptr);
}

void cancel_carousel() {
  if (g_carousel_timer) { lv_timer_del(g_carousel_timer); g_carousel_timer = nullptr; }
}

void schedule_carousel_for(int idx) {
  cancel_carousel();
  if (idx < 0 || idx >= (int)pages.size()) return;
  uint32_t ms = pages[idx].auto_advance_ms;
  if (ms == 0) return;
  // Repeating timer is fine here: we explicitly cancel it on every page
  // switch (in render_page → schedule_carousel_for), so the only firing
  // it ever does is the next single tick before being replaced.
  g_carousel_timer = lv_timer_create(carousel_advance_cb, ms, nullptr);
}

/// Pick the active page. show_when pages have priority — the first one whose
/// expression is truthy wins and the carousel pauses. If none match, fall
/// back to the first carousel page (or just the first page if none of those
/// either). Switch only if the choice differs from the current page.
void reevaluate_active_page() {
  if (pages.empty()) return;

  // Priority: any page whose show_when matches.
  for (size_t i = 0; i < pages.size(); ++i) {
    const auto& p = pages[i];
    if (p.show_when.length() > 0 && eval_expr(p.show_when)) {
      if ((int)i != current_page_idx) render_page((int)i);
      return;
    }
  }

  // Otherwise: a carousel page (or first page if no carousel exists).
  // If we're already on a carousel page, leave it alone — the carousel
  // timer will advance us in due course.
  if (current_page_idx >= 0 && pages[current_page_idx].show_when.length() == 0) return;
  for (size_t i = 0; i < pages.size(); ++i) {
    if (pages[i].show_when.length() == 0) { render_page((int)i); return; }
  }
  render_page(0);
}

void on_datasource_change(const String& path) {
  // First: refresh widgets on the current page that bind to or react to this path.
  for (auto& w : bound_widgets) {
    if (w.binding == path || (w.hidden_when.length() && w.hidden_when.indexOf(path) >= 0)) {
      update_widget_value(w);
    }
  }
  // Second: page-level — a datasource change might flip a show_when result.
  // Cheap to re-evaluate all pages on every change at this scale.
  reevaluate_active_page();
}

}  // namespace

String apply_layout(const JsonDocument& doc) {
  String ver = doc["schema_version"].as<String>();
  if (ver != "0.1" && ver != "0.2") return "unsupported schema_version: " + ver;

  // Rotation: read either page.rotation (0.1) or pages[0].rotation (0.2).
  int rot_deg = -1;
  if (doc["page"]["rotation"].is<int>())            rot_deg = doc["page"]["rotation"].as<int>();
  else if (doc["pages"][0]["rotation"].is<int>())   rot_deg = doc["pages"][0]["rotation"].as<int>();
  if (rot_deg >= 0) {
    int wanted = (rot_deg / 90) & 3;
    if (wanted != settings::get_rotation()) {
      active_doc.clear(); active_doc.set(doc.as<JsonVariantConst>()); persist_active();
      settings::set_rotation(wanted);
      Serial.printf("[render] rotation change persisted (%d), restart pending\n", wanted);
      g_restart_pending_at_ms = millis() + 250;
      return "";
    }
  }

  if (doc["datasources"].is<JsonArrayConst>())
    datasources::configure(doc["datasources"].as<JsonArrayConst>());

  if (!change_handler_installed) {
    datasources::on_change(on_datasource_change);
    change_handler_installed = true;
  }

  // Cache the document FIRST so page_object_at() can read from it.
  active_doc.clear();
  active_doc.set(doc.as<JsonVariantConst>());

  // Build the page registry.
  pages.clear();
  current_page_idx = -1;
  if (active_doc["pages"].is<JsonArrayConst>()) {
    int i = 0;
    for (JsonObjectConst p : active_doc["pages"].as<JsonArrayConst>()) {
      PageDef def;
      def.id        = p["id"].is<const char*>() ? String(p["id"].as<const char*>())
                                                : (String("page-") + i);
      def.show_when = p["show_when"].is<const char*>() ? String(p["show_when"].as<const char*>())
                                                       : String("");
      def.auto_advance_ms = (uint32_t)(p["auto_advance_sec"] | 0) * 1000U;
      def.transition      = String(p["transition"] | "");
      def.doc_index = i;
      pages.push_back(def);
      ++i;
    }
  } else if (active_doc["widgets"].is<JsonArrayConst>()) {
    PageDef def;
    def.id        = "default";
    def.show_when = "";
    def.doc_index = 0;
    pages.push_back(def);
  } else {
    return "layout has neither pages[] nor widgets[]";
  }

  reevaluate_active_page();
  return "";
}

// NVS strings are capped at ~3800 bytes; split larger layouts into chunks.
static const int NVS_CHUNK = 3800;

void persist_active() {
  if (active_doc.isNull()) return;
  String s;
  serializeJson(active_doc, s);
  Preferences p;
  p.begin("cydlayout", false);
  p.clear();
  int n = (s.length() + NVS_CHUNK - 1) / NVS_CHUNK;
  p.putInt("n", n);
  for (int i = 0; i < n; i++) {
    char key[6]; snprintf(key, sizeof(key), "l%d", i);
    p.putString(key, s.substring(i * NVS_CHUNK, min((i + 1) * NVS_CHUNK, (int)s.length())));
  }
  p.end();
}

bool load_persisted() {
  Preferences p;
  p.begin("cydlayout", true);
  int n = p.getInt("n", 0);
  String s;
  if (n == 0) {
    s = p.getString("layout", "");  // legacy single-key format
  } else {
    for (int i = 0; i < n; i++) {
      char key[6]; snprintf(key, sizeof(key), "l%d", i);
      s += p.getString(key, "");
    }
  }
  p.end();
  if (s.length() == 0) return false;
  JsonDocument doc;
  if (deserializeJson(doc, s)) return false;
  return apply_layout(doc).length() == 0;
}

void shutdown() {
  cancel_carousel();
  clear_bound_widgets();
  if (current_screen) { lv_obj_del(current_screen); current_screen = nullptr; }
  pages.clear();
  current_page_idx = -1;
  active_doc.clear();
}

bool pending_restart() {
  return g_restart_pending_at_ms != 0 && millis() >= g_restart_pending_at_ms;
}

}  // namespace cydstudio::renderer
