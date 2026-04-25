/**
 * Renderer — translates a layout JSON into LVGL widgets and keeps them in
 * sync with datasource changes.
 *
 * Phase 1 widgets: label, bar, arc, image, line, button.
 * Image widget supports "builtin:claude-logo" only (Phase 1); http(s):// and
 * data:image/...;base64 come in Phase 2.
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
  lv_obj_t* obj      = nullptr;
  String    type;
  String    binding;
  String    fmt;
  String    static_text;     // for label fallback when binding is null/missing
  String    hidden_when;
  float     min = 0, max = 100;
};

lv_obj_t*                current_screen = nullptr;
std::vector<BoundWidget> bound_widgets;
JsonDocument             active_doc;
bool                     change_handler_installed = false;
uint32_t                 g_restart_pending_at_ms = 0;

uint32_t hex_to_lv_u32(const char* s, uint32_t fallback) {
  if (!s || s[0] != '#' || strlen(s) != 7) return fallback;
  return (uint32_t)strtoul(s + 1, nullptr, 16);
}
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

bool eval_hidden(const String& expr) {
  if (expr.length() == 0) return false;
  // v0.1 supports only "<binding>" and "!<binding>" (truthiness).
  String e = expr; e.trim();
  bool negate = false;
  if (e.startsWith("!")) { negate = true; e = e.substring(1); e.trim(); }
  JsonDocument tmp; JsonVariant v = tmp.to<JsonVariant>();
  bool found = datasources::resolve(e, v);
  bool truthy = false;
  if (found) {
    if (v.is<bool>())   truthy = v.as<bool>();
    else if (v.is<int>())    truthy = v.as<int>() != 0;
    else if (v.is<float>())  truthy = v.as<float>() != 0.0f;
    else if (v.is<const char*>()) truthy = v.as<String>().length() > 0;
    else                     truthy = !v.isNull();
  }
  return negate ? !truthy : truthy;
}

void update_widget_value(BoundWidget& w) {
  if (w.hidden_when.length()) {
    bool hide = eval_hidden(w.hidden_when);
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
    lv_label_set_text(w.obj, txt.c_str());
  } else if (w.type == "bar") {
    int val = found ? v.as<int>() : (int)w.min;
    lv_bar_set_value(w.obj, val, LV_ANIM_OFF);
  } else if (w.type == "arc") {
    int val = found ? v.as<int>() : (int)w.min;
    lv_arc_set_value(w.obj, val);
  }
}

void on_datasource_change(const String& path) {
  for (auto& w : bound_widgets) {
    if (w.binding == path || w.hidden_when.indexOf(path) >= 0) {
      update_widget_value(w);
    }
  }
}

void build_label(JsonObjectConst spec, lv_obj_t* parent) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_text_font(lbl, font_for(spec["font"] | 14), 0);
  lv_obj_set_style_text_color(lbl, hex(spec["color"], lv_color_white()), 0);
  const char* align = spec["align"] | "left";
  int x = spec["x"] | 0;
  int y = spec["y"] | 0;
  // Semantics: when align=right, x is the *right* edge; when align=center, x is the center.
  // Without an explicit width we anchor to the screen edge (works for typical layouts).
  if (strcmp(align, "right") == 0) {
    int w = spec["w"] | x;                // span from 0 to x
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

  BoundWidget w;
  w.obj         = lbl;
  w.type        = "label";
  w.binding     = spec["binding"].as<String>();
  w.fmt         = spec["format"].as<String>();
  w.static_text = spec["text"].as<String>();
  w.hidden_when = spec["hidden_when"].as<String>();
  if (w.binding.length() == 0) lv_label_set_text(lbl, w.static_text.c_str());
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
  lv_obj_set_style_bg_color(bar, hex(spec["color"], lv_color_hex(0xD95A12)), LV_PART_INDICATOR);
  if (!(spec["rounded"] | true)) {
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
  }

  BoundWidget w;
  w.obj         = bar;
  w.type        = "bar";
  w.binding     = spec["binding"].as<String>();
  w.hidden_when = spec["hidden_when"].as<String>();
  w.min = mn; w.max = mx;
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
  w.binding     = spec["binding"].as<String>();
  w.hidden_when = spec["hidden_when"].as<String>();
  w.min = mn; w.max = mx;
  bound_widgets.push_back(w);
  update_widget_value(bound_widgets.back());
}

void build_line(JsonObjectConst spec, lv_obj_t* parent) {
  static lv_point_t pts_storage[64][2];
  static int pts_idx = 0;
  if (pts_idx >= 64) return;  // limit
  pts_storage[pts_idx][0] = { (lv_coord_t)(spec["x1"] | 0), (lv_coord_t)(spec["y1"] | 0) };
  pts_storage[pts_idx][1] = { (lv_coord_t)(spec["x2"] | 0), (lv_coord_t)(spec["y2"] | 0) };
  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, pts_storage[pts_idx], 2);
  pts_idx++;
  lv_obj_set_style_line_color(line, hex(spec["color"], lv_color_hex(0x444444)), 0);
  lv_obj_set_style_line_width(line, spec["thickness"] | 1, 0);
}

void build_image(JsonObjectConst spec, lv_obj_t* parent) {
  // Phase 1: only "builtin:claude-logo" supported. http/data come later.
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

}  // namespace

String apply_layout(const JsonDocument& doc) {
  if (doc["schema_version"] != "0.1") return "unsupported schema_version";
  if (!doc["widgets"].is<JsonArrayConst>()) return "missing widgets[]";

  // Rotation change: persist + signal a deferred restart. We can't reboot
  // synchronously here because the HTTP response hasn't been flushed yet —
  // main.cpp polls renderer::pending_restart() and reboots from loop().
  if (doc["page"]["rotation"].is<int>()) {
    int wanted = (doc["page"]["rotation"].as<int>() / 90) & 3;
    if (wanted != settings::get_rotation()) {
      active_doc.clear(); active_doc.set(doc.as<JsonVariantConst>()); persist_active();
      settings::set_rotation(wanted);
      Serial.printf("[render] rotation change persisted (%d), restart pending\n", wanted);
      g_restart_pending_at_ms = millis() + 250;
      return "";  // accept the layout; reboot will happen after response flush
    }
  }

  // Reconfigure datasources (idempotent).
  if (doc["datasources"].is<JsonArrayConst>())
    datasources::configure(doc["datasources"].as<JsonArrayConst>());

  if (!change_handler_installed) {
    datasources::on_change(on_datasource_change);
    change_handler_installed = true;
  }

  // Build new screen first, then load it, then delete the old one.
  // LVGL forbids deleting the active screen, so we must swap first; we also
  // clear bound_widgets before the delete so any stray callback can't follow
  // dangling pointers into freed widgets.
  lv_obj_t* prev_screen = current_screen;
  bound_widgets.clear();

  current_screen = lv_obj_create(nullptr);
  // Nuke all theme-applied styling (padding, border, scrollbar reservations)
  // so that layout (x,y) really means "(x,y) from screen origin".
  lv_obj_remove_style_all(current_screen);
  lv_obj_set_style_bg_color(current_screen, hex(doc["page"]["bg"], lv_color_hex(0x111111)), 0);
  lv_obj_set_style_bg_opa(current_screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(current_screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_scr_load(current_screen);

  if (prev_screen) lv_obj_del(prev_screen);

  for (JsonObjectConst w : doc["widgets"].as<JsonArrayConst>())
    build_widget(w, current_screen);

  // Cache for persistence (assignment, not set() — set() doesn't take JsonDocument).
  active_doc.clear();
  active_doc.set(doc.as<JsonVariantConst>());
  return "";
}

void persist_active() {
  if (active_doc.isNull()) return;
  String s;
  serializeJson(active_doc, s);
  Preferences p;
  p.begin("cydlayout", false);
  p.putString("layout", s);
  p.end();
}

bool load_persisted() {
  Preferences p;
  p.begin("cydlayout", true);
  String s = p.getString("layout", "");
  p.end();
  if (s.length() == 0) return false;
  JsonDocument doc;
  if (deserializeJson(doc, s)) return false;
  return apply_layout(doc).length() == 0;
}

void shutdown() {
  if (current_screen) { lv_obj_del(current_screen); current_screen = nullptr; }
  bound_widgets.clear();
  active_doc.clear();
}

bool pending_restart() {
  return g_restart_pending_at_ms != 0 && millis() >= g_restart_pending_at_ms;
}

}  // namespace cydstudio::renderer
