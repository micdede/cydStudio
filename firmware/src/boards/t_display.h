/**
 * Board profile: LILYGO TTGO T-Display (ESP32 + 1.14" ST7789, 135x240).
 *
 * Pin assignments are from LILYGO's published example sketches and the
 * Bodmer/TFT_eSPI Setup25 reference. The 135x240 ST7789 needs CGRAM_OFFSET
 * because the panel is windowed inside the controller's 240x320 frame.
 *
 * **Status: unverified on hardware**. If your display shows garbage after
 * the first flash, the offsets or rotation values likely need adjusting —
 * compare against TFT_eSPI's User_Setups/Setup25_TTGO_T_Display.h.
 */
#pragma once

#define BOARD_W 135
#define BOARD_H 240

#include <TFT_eSPI.h>

class BoardDeviceTFT_TDisplay {
  TFT_eSPI _tft;

 public:
  void init() {
    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(TFT_BLACK);
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif
  }
  void setRotation(int r)   { _tft.setRotation(r & 3); }
  void setBrightness(uint8_t lvl) {
#ifdef TFT_BL
    digitalWrite(TFT_BL, lvl > 0 ? HIGH : LOW);
#else
    (void)lvl;
#endif
  }
  void fillScreen(uint16_t c)                          { _tft.fillScreen(c); }
  void setTextColor(uint16_t fg, uint16_t bg)          { _tft.setTextColor(fg, bg); }
  void setCursor(int x, int y)                         { _tft.setCursor(x, y); }
  void printf(const char* fmt, ...) {
    char buf[128]; va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    _tft.print(buf);
  }
  void fillRect(int x, int y, int w, int h, uint16_t c){ _tft.fillRect(x, y, w, h, c); }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b)   { return _tft.color565(r, g, b); }
  int  width()                                          { return _tft.width(); }
  int  height()                                         { return _tft.height(); }
  void startWrite()                                     { _tft.startWrite(); }
  void endWrite()                                       { _tft.endWrite(); }
  void setAddrWindow(int x, int y, int w, int h)        { _tft.setAddrWindow(x, y, w, h); }
  void writePixels(uint16_t* px, uint32_t n)            { _tft.pushPixels(px, n); }
};
