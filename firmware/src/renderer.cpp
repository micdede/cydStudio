/**
 * Renderer — translates a layout JSON into LVGL widgets and keeps them in
 * sync with datasource changes.
 *
 * STATUS: stubbed. Phase 1 work:
 *   - implement build_widget() per type (label, bar, arc, image, button, line, sparkline)
 *   - hook datasources::on_change to invalidate bound widgets
 *   - implement format-string substitution incl. {duration} for seconds-to-h:mm
 *   - persist to NVS under key "layout"; load on boot
 */
#include "renderer.h"

#include <Preferences.h>
#include <lvgl.h>

namespace cydstudio::renderer {

static lv_obj_t* current_screen = nullptr;

String apply_layout(const JsonDocument& doc) {
  if (doc["schema_version"] != "0.1") return "unsupported schema_version";
  // TODO Phase 1: build widget tree.
  if (current_screen) lv_obj_del(current_screen);
  current_screen = lv_obj_create(nullptr);
  lv_scr_load(current_screen);
  auto* placeholder = lv_label_create(current_screen);
  lv_label_set_text(placeholder, "renderer stub — got layout");
  lv_obj_center(placeholder);
  return "";
}

void persist_active() {
  // TODO Phase 1: serialize current layout JSON to NVS key "layout".
}

bool load_persisted() {
  // TODO Phase 1: read NVS, deserialize, call apply_layout.
  return false;
}

void shutdown() {
  if (current_screen) {
    lv_obj_del(current_screen);
    current_screen = nullptr;
  }
}

}  // namespace cydstudio::renderer
