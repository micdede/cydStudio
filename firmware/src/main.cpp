/**
 * cydStudio firmware entry point.
 *
 * Boot sequence:
 *  1. Bring up display, run a 2-second test pattern (proves board profile is right).
 *  2. Load provisioning config from NVS.
 *     - If absent: start SoftAP, wait for /provision POST, save, restart.
 *     - If present: connect to WiFi, start mDNS + HTTP server + OTA.
 *  3. Try to load the last-good layout from NVS; otherwise show a placeholder.
 *  4. Loop: lv_timer_handler, datasources::tick, server.handleClient, ArduinoOTA.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lvgl.h>

#include "boards/dispatch.h"
#include "datasources.h"
#include "provisioning.h"
#include "renderer.h"

static BoardDisplay  display;
static WebServer     server(80);
static lv_color_t*   draw_buf_a = nullptr;
static const int     DRAW_BUF_LINES = 40;

// --- LVGL flush callback bridging to LovyanGFX -----------------------------
static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;
  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, w, h);
  display.writePixels(reinterpret_cast<uint16_t*>(px), w * h);
  display.endWrite();
  lv_disp_flush_ready(drv);
}

// --- 2-second board sanity check; user can verify the right profile ran ----
static void test_pattern() {
  display.fillScreen(0);
  display.setTextColor(0xFFFF, 0);
  display.setCursor(8, 8);
  display.printf("cydStudio\n%s\n%dx%d", BOARD_ID, BOARD_W, BOARD_H);
  for (int i = 0; i < 16; ++i) {
    int y = 60 + i * 4;
    if (y >= BOARD_H) break;
    display.fillRect(0, y, BOARD_W, 4, display.color565(i * 16, 0, 255 - i * 16));
  }
  delay(2000);
  display.fillScreen(0);
}

// --- HTTP routes ----------------------------------------------------------
static void register_routes() {
  server.on("/health", HTTP_GET, [] {
    server.send(200, "application/json", R"({"ok":true,"board":")" + String(BOARD_ID) + "\"}");
  });
  server.on("/layout", HTTP_POST, [] {
    JsonDocument doc;
    auto err = deserializeJson(doc, server.arg("plain"));
    if (err) { server.send(400, "text/plain", err.c_str()); return; }
    auto msg = cydstudio::renderer::apply_layout(doc);
    if (msg.length()) { server.send(400, "text/plain", msg); return; }
    cydstudio::renderer::persist_active();
    server.send(200, "application/json", R"({"ok":true})");
  });
  server.on(UriBraces("/data/{}"), HTTP_POST, [] {
    String id = server.pathArg(0);
    JsonDocument body;
    auto err = deserializeJson(body, server.arg("plain"));
    if (err) { server.send(400, "text/plain", err.c_str()); return; }
    auto msg = cydstudio::datasources::accept_push(id, body);
    if (msg.length()) { server.send(400, "text/plain", msg); return; }
    server.send(200, "application/json", R"({"ok":true})");
  });
}

// --- Setup ----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  display.init();
  display.setBrightness(255);
  display.setRotation(0);
  test_pattern();

  // LVGL init
  lv_init();
  draw_buf_a = static_cast<lv_color_t*>(
      heap_caps_malloc(BOARD_W * DRAW_BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA));
  static lv_disp_draw_buf_t draw_buf;
  lv_disp_draw_buf_init(&draw_buf, draw_buf_a, nullptr, BOARD_W * DRAW_BUF_LINES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = BOARD_W;
  disp_drv.ver_res  = BOARD_H;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Provisioning
  auto cfg = cydstudio::provisioning::load();
  if (!cfg.valid) {
    Serial.println("[boot] no provisioning config — entering SoftAP");
    cydstudio::provisioning::run_first_boot_ap(String(BOARD_ID));
    ESP.restart();
  }

  // WiFi + mDNS
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname.c_str());
  WiFi.begin(cfg.ssid.c_str(), cfg.psk.c_str());
  while (WiFi.status() != WL_CONNECTED) { delay(250); }
  MDNS.begin(cfg.hostname.c_str());
  MDNS.addService("cydstudio", "tcp", 80);

  // OTA
  ArduinoOTA.setHostname(cfg.hostname.c_str());
  if (cfg.ota_password.length()) ArduinoOTA.setPassword(cfg.ota_password.c_str());
  ArduinoOTA.begin();

  // HTTP
  register_routes();
  server.begin();

  // Last-good layout (or placeholder)
  if (!cydstudio::renderer::load_persisted()) {
    auto* lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl, "cydStudio\nwaiting for first layout…");
    lv_obj_center(lbl);
  }
}

// --- Loop ------------------------------------------------------------------
void loop() {
  lv_timer_handler();
  cydstudio::datasources::tick();
  server.handleClient();
  ArduinoOTA.handle();
  delay(2);
}
