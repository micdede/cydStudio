#pragma once
#include <lvgl.h>

// Look up a built-in image by name. Returns nullptr if not found.
// Implementation in builtin_images.cpp; image data lives in src/assets/generated/.
const lv_img_dsc_t* lookup_builtin_image(const char* name);
