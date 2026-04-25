/**
 * Board dispatcher — picks the right device class based on the
 * CYDSTUDIO_BOARD_* macro set by platformio.ini. Each clause references one
 * header that defines BOARD_W/BOARD_H and a class with the minimal API the
 * display HAL (display.cpp) calls.
 *
 * Most boards use the TFT_eSPI-based wrappers below. AMOLED boards need
 * LovyanGFX (different driver stack) and will land in a separate
 * boards/t_display_s3_amoled.h once the LovyanGFX/LVGL link conflict is
 * worked around.
 */
#pragma once

#if defined(CYDSTUDIO_BOARD_CYD_2432S028) || defined(CYDSTUDIO_BOARD_CYD_2432S028R)
  #include "cyd_2432s028.h"
  using BoardDisplay = BoardDeviceTFT;
  static constexpr const char* BOARD_ID = "cyd-2432s028";

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_DISPLAY)
  #include "t_display.h"
  using BoardDisplay = BoardDeviceTFT_TDisplay;
  static constexpr const char* BOARD_ID = "t-display";

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_DISPLAY_S3)
  #include "t_display_s3.h"
  using BoardDisplay = BoardDeviceTFT_TDisplayS3;
  static constexpr const char* BOARD_ID = "t-display-s3";

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_QT_PRO)
  #include "t_qt_pro.h"
  using BoardDisplay = BoardDeviceTFT_TQTPro;
  static constexpr const char* BOARD_ID = "t-qt-pro";

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_DISPLAY_S3_AMOLED)
  #error "AMOLED profile pending: needs LovyanGFX + a workaround for the bundled-LVGL link conflict. Tracked as Phase 5.4 follow-up."

#else
  #error "No CYDSTUDIO_BOARD_* macro defined. Set one in platformio.ini build_flags."
#endif
