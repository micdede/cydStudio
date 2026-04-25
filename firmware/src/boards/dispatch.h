/**
 * Board dispatcher — picks the right LovyanGFX device class based on the
 * CYDSTUDIO_BOARD_* macro set by platformio.ini. Adding a board: drop a
 * new boards/<board>.h, then add a clause here.
 */
#pragma once

#if defined(CYDSTUDIO_BOARD_CYD_2432S028) || defined(CYDSTUDIO_BOARD_CYD_2432S028R)
  #include "cyd_2432s028.h"
  using BoardDisplay = Board_CYD_2432S028;
  static constexpr const char* BOARD_ID = "cyd-2432s028";

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_DISPLAY)
  // TODO: implement board profile
  #error "T-Display board profile not yet implemented (boards/t_display.h)"

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_DISPLAY_S3)
  #error "T-Display S3 board profile not yet implemented (boards/t_display_s3.h)"

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_QT_PRO)
  #error "T-QT Pro board profile not yet implemented (boards/t_qt_pro.h)"

#elif defined(CYDSTUDIO_BOARD_LILYGO_T_DISPLAY_S3_AMOLED)
  #error "T-Display S3 AMOLED board profile not yet implemented (boards/t_display_s3_amoled.h)"

#else
  #error "No CYDSTUDIO_BOARD_* macro defined. Set one in platformio.ini build_flags."
#endif
