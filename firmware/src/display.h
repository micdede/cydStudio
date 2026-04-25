/**
 * Display HAL — thin C-style facade over the per-board LovyanGFX device.
 *
 * This file is the *only* header that touches LovyanGFX. main.cpp/renderer.cpp
 * include this and lvgl.h side-by-side; they must NOT include LovyanGFX
 * directly because LovyanGFX bundles its own copy of LVGL font headers,
 * which conflict with the standalone LVGL we use for widgets.
 */
#pragma once
#include <stdint.h>

namespace cydstudio::display {

void init(int rotation_quarter);  // 0,1,2,3 == 0°,90°,180°,270°
void set_brightness(uint8_t level);  // 0..255
void test_pattern(const char* board_id);
void flush(int x, int y, int w, int h, uint16_t* pixels);
int  width();
int  height();
const char* board_id();

}  // namespace cydstudio::display
