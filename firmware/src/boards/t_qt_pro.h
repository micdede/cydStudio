/**
 * Board profile: LILYGO T-QT Pro (ESP32-S3 + 0.85" GC9107, 128x128).
 *
 * GC9107 isn't a first-class TFT_eSPI driver, but it is wire-compatible with
 * the GC9A01 driver path for the init/draw loop — TFT_eSPI's GC9A01_DRIVER
 * works as long as TFT_WIDTH/TFT_HEIGHT and the mirror/rotation flags are
 * set correctly. Backlight pin is shared with the TPS LDO enable, so
 * "BL HIGH" also powers the display rail on this board.
 *
 * **Status: unverified on hardware** — try the test pattern after first
 * flash and if it's mirrored or off-position, adjust TFT_RGB_ORDER /
 * MADCTL flags in platformio.ini.
 */
#pragma once

#define BOARD_W 128
#define BOARD_H 128

#include <TFT_eSPI.h>

class BoardDeviceTFT_TQTPro {
  TFT_eSPI _tft;

 public:
  void init() {
    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(TFT_BLACK);
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);  // BL = LDO enable on this board
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
