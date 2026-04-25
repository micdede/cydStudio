/**
 * cydStudio firmware entry point.
 *
 * Boot sequence:
 *  1. Bring up display, run a 2-second test pattern (proves board profile is right).
 *  2. Load provisioning config from NVS (or compile-time fallback via secrets.h).
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
#include <uri/UriBraces.h>
#include <WiFi.h>
#include <lvgl.h>

#include "datasources.h"
#include "display.h"
#include "provisioning.h"
#include "renderer.h"
#include "serial_provision.h"
#include "settings.h"

static WebServer   server(80);
static lv_color_t* draw_buf_a = nullptr;
static const int   DRAW_BUF_LINES = 40;

static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;
  cydstudio::display::flush(area->x1, area->y1, w, h, reinterpret_cast<uint16_t*>(px));
  lv_disp_flush_ready(drv);
}

static void register_routes() {
  server.on("/health", HTTP_GET, [] {
    server.send(200, "application/json",
                String(R"({"ok":true,"board":")") + cydstudio::display::board_id() + "\"}");
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

void setup() {
  Serial.begin(115200);
  // Listen briefly on Serial for a host-provided PROVISION line. Used by
  // the Mac app right after a USB flash so the user never has to do the
  // SoftAP dance. Cheap (~3s) and only blocks before any hardware init.
  cydstudio::serial_provision::run(/*timeout_ms=*/3000);

  cydstudio::display::init(cydstudio::settings::get_rotation());
  cydstudio::display::test_pattern(cydstudio::display::board_id());

  lv_init();
  const int W = cydstudio::display::width();
  const int H = cydstudio::display::height();
  draw_buf_a = static_cast<lv_color_t*>(
      heap_caps_malloc(W * DRAW_BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA));
  static lv_disp_draw_buf_t draw_buf;
  lv_disp_draw_buf_init(&draw_buf, draw_buf_a, nullptr, W * DRAW_BUF_LINES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = W;
  disp_drv.ver_res  = H;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  auto cfg = cydstudio::provisioning::load();
  if (!cfg.valid) {
    Serial.println("[boot] no provisioning config — entering SoftAP");
    cydstudio::provisioning::run_first_boot_ap(cydstudio::display::board_id());
    ESP.restart();
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname.c_str());
  WiFi.begin(cfg.ssid.c_str(), cfg.psk.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) { delay(250); }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] %s\n", WiFi.localIP().toString().c_str());
    // Kick off SNTP so the internal clock datasource has a real time to read.
    // The TZ string comes from each clock datasource's config; here we only
    // start the sync.
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    MDNS.begin(cfg.hostname.c_str());
    MDNS.addService("cydstudio", "tcp", 80);

    ArduinoOTA.setHostname(cfg.hostname.c_str());
    if (cfg.ota_password.length()) ArduinoOTA.setPassword(cfg.ota_password.c_str());
    ArduinoOTA.begin();

    register_routes();
    server.begin();
  } else {
    Serial.println("[wifi] failed — running offline");
  }

  if (!cydstudio::renderer::load_persisted()) {
    auto* lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl, "cydStudio\nwaiting for first layout…");
    lv_obj_center(lbl);
  }
}

void loop() {
  lv_timer_handler();
  cydstudio::datasources::tick();
  server.handleClient();
  ArduinoOTA.handle();
  if (cydstudio::renderer::pending_restart()) {
    Serial.println("[boot] settings change → restart");
    delay(50);
    ESP.restart();
  }
  delay(2);
}
