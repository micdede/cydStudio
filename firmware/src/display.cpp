/**
 * Display HAL implementation. The only .cpp that includes LovyanGFX.
 */
#include "display.h"

#include <Arduino.h>
#include "boards/dispatch.h"

namespace cydstudio::display {

namespace {
BoardDisplay device;
}

void init(int rotation_quarter) {
  device.init();
  device.setRotation(rotation_quarter & 3);
  device.setBrightness(255);
  device.fillScreen(0);
}

void set_brightness(uint8_t level) { device.setBrightness(level); }

int width()  { return device.width(); }
int height() { return device.height(); }
const char* board_id() { return BOARD_ID; }

void flush(int x, int y, int w, int h, uint16_t* pixels) {
  device.startWrite();
  device.setAddrWindow(x, y, w, h);
  device.writePixels(pixels, w * h);
  device.endWrite();
}

void test_pattern(const char* board_id) {
  const int W = device.width();
  const int H = device.height();
  device.fillScreen(0);
  device.setTextColor(0xFFFF, 0);
  device.setCursor(8, 8);
  device.printf("cydStudio\n%s\n%dx%d", board_id, W, H);
  for (int i = 0; i < 16; ++i) {
    int y = 60 + i * 4;
    if (y >= H) break;
    device.fillRect(0, y, W, 4, device.color565(i * 16, 0, 255 - i * 16));
  }
  delay(2000);
  device.fillScreen(0);
}

}  // namespace cydstudio::display
