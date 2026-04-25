/**
 * Board profile: ESP32-2432S028 ("CYD"), 240x320 ILI9341.
 * Driver: TFT_eSPI, configured via build_flags in platformio.ini.
 */
#pragma once

#define BOARD_W 240
#define BOARD_H 320

#include <TFT_eSPI.h>

class BoardDeviceTFT {
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
  void setRotation(int r)   { _tft.setRotation(r); }
  void setBrightness(uint8_t lvl) {
#ifdef TFT_BL
    // CYD has the BL on a plain GPIO; PWM would need ledc setup. Treat 0=off, >0=on.
    digitalWrite(TFT_BL, lvl > 0 ? HIGH : LOW);
#else
    (void)lvl;
#endif
  }
  void fillScreen(uint16_t c) { _tft.fillScreen(c); }
  void setTextColor(uint16_t fg, uint16_t bg) { _tft.setTextColor(fg, bg); }
  void setCursor(int x, int y) { _tft.setCursor(x, y); }
  void printf(const char* fmt, ...) {
    char buf[128];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    _tft.print(buf);
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) { _tft.fillRect(x, y, w, h, c); }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return _tft.color565(r, g, b); }
  int width()  { return _tft.width(); }
  int height() { return _tft.height(); }
  void startWrite() { _tft.startWrite(); }
  void endWrite()   { _tft.endWrite(); }
  void setAddrWindow(int x, int y, int w, int h) { _tft.setAddrWindow(x, y, w, h); }
  void writePixels(uint16_t* px, uint32_t n) { _tft.pushPixels(px, n); }
};
