/**
 * Built-in image registry — images compiled into the firmware that any
 * layout can reference via "builtin:<name>". Add new images by extending
 * the table below.
 *
 * Phase 1 ships with the Claude Code logo migrated from the old monitor.
 * Run scripts/convert_image.py to regenerate the C array if you swap the
 * source artwork.
 */
#include <Arduino.h>
#include <lvgl.h>
#include <string.h>

// Forward decl — definitions live in src/assets/generated/*.c
extern const lv_img_dsc_t claude_logo_dsc;

struct Entry { const char* name; const lv_img_dsc_t* dsc; };
static const Entry kBuiltins[] = {
  { "claude-logo", &claude_logo_dsc },
};

const lv_img_dsc_t* lookup_builtin_image(const char* name) {
  if (!name) return nullptr;
  for (auto& e : kBuiltins) {
    if (strcmp(e.name, name) == 0) return e.dsc;
  }
  return nullptr;
}
