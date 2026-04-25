/**
 * Board profile: LILYGO T-Display S3 (ESP32-S3 + 1.9" ST7789, 170x320).
 *
 * Drives the panel in 8-bit parallel ("8080") mode — much faster than SPI
 * and the only mode this board exposes. PWR_ON (GPIO15) must be driven HIGH
 * before the display will power on; we do that in init().
 *
 * **Status: unverified on hardware**. Pin map comes from LILYGO's
 * TDisplay-S3 sample sketches.
 */
#pragma once

#define BOARD_W 170
#define BOARD_H 320
#define TDISPLAY_S3_PWR_ON_PIN 15

#include <TFT_eSPI.h>

class BoardDeviceTFT_TDisplayS3 {
  TFT_eSPI _tft;

 public:
  void init() {
    pinMode(TDISPLAY_S3_PWR_ON_PIN, OUTPUT);
    digitalWrite(TDISPLAY_S3_PWR_ON_PIN, HIGH);  // must be HIGH or display stays dark
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
