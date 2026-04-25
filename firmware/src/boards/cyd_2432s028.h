/**
 * Board profile: ESP32-2432S028 ("CYD"), 240x320 ILI9341 over SPI.
 * No touch on the plain S028; the S028R variant adds an XPT2046 — wire it
 * in through this same header guarded by CYDSTUDIO_BOARD_CYD_2432S028R.
 */
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define BOARD_W 240
#define BOARD_H 320
#define BOARD_BL_PIN 21
#define BOARD_BL_PWM_FREQ 5000

class Board_CYD_2432S028 : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
#ifdef CYDSTUDIO_BOARD_CYD_2432S028R
  lgfx::Touch_XPT2046 _touch;
#endif

 public:
  Board_CYD_2432S028() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = HSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 14;
      cfg.pin_mosi    = 13;
      cfg.pin_miso    = 12;
      cfg.pin_dc      =  2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = -1;
      cfg.pin_busy = -1;
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable    = true;
      cfg.invert      = false;
      cfg.rgb_order   = false;
      cfg.dlen_16bit  = false;
      cfg.bus_shared  = true;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl      = BOARD_BL_PIN;
      cfg.invert      = false;
      cfg.freq        = BOARD_BL_PWM_FREQ;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
#ifdef CYDSTUDIO_BOARD_CYD_2432S028R
    {
      auto cfg = _touch.config();
      cfg.x_min = 0; cfg.x_max = 239;
      cfg.y_min = 0; cfg.y_max = 319;
      cfg.pin_int  = 36;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.spi_host = VSPI_HOST;
      cfg.freq     = 1000000;
      cfg.pin_sclk = 25;
      cfg.pin_mosi = 32;
      cfg.pin_miso = 39;
      cfg.pin_cs   = 33;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
#endif
    setPanel(&_panel);
  }
};
