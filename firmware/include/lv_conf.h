/**
 * Minimal lv_conf.h for LVGL 8.3 used across all cydStudio boards.
 * Color depth and swap are tuned for LovyanGFX's RGB565 output.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ---- Color settings ---- */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/* ---- Memory ---- */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (96U * 1024U)

/* ---- HAL ---- */
#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DPI_DEF 130

/* ---- Features ---- */
#define LV_USE_ANIMATION 1
#define LV_USE_SHADOW 1
#define LV_USE_BLEND_MODES 1
#define LV_USE_OPA_SCALE 1
#define LV_USE_IMG_TRANSFORM 1

/* ---- Fonts ---- */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ---- Widgets ---- */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_LABEL 1
#define LV_LABEL_TEXT_SELECTION 0
#define LV_LABEL_LONG_TXT_HINT 1
#define LV_USE_LINE 1
#define LV_USE_OBJ 1
#define LV_USE_IMG 1
#define LV_USE_CHART 1

/* ---- Theme ---- */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* ---- Logging ---- */
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

/* ---- Misc ---- */
#define LV_USE_USER_DATA 1
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif /* LV_CONF_H */
