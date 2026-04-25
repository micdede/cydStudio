#include "serial_provision.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "provisioning.h"

namespace cydstudio::serial_provision {

void run(uint32_t timeout_ms) {
  Serial.println();
  Serial.println(F("CYDSTUDIO_PROVISION_READY"));

  String line;
  uint32_t deadline = millis() + timeout_ms;
  while (millis() < deadline) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (line.startsWith("PROVISION ")) {
          String body = line.substring(strlen("PROVISION "));
          JsonDocument doc;
          auto err = deserializeJson(doc, body);
          if (err) {
            Serial.printf("PROVISION_ERR parse: %s\n", err.c_str());
            line = "";
            continue;
          }
          if (!doc["ssid"].is<const char*>() || !doc["hostname"].is<const char*>()) {
            Serial.println(F("PROVISION_ERR missing ssid or hostname"));
            line = "";
            continue;
          }
          provisioning::Config cfg;
          cfg.ssid         = doc["ssid"].as<const char*>();
          cfg.psk          = doc["psk"]     | "";
          cfg.hostname     = doc["hostname"].as<const char*>();
          cfg.ota_password = doc["ota_pw"]  | "";
          provisioning::save(cfg);
          Serial.println(F("PROVISION_OK"));
          delay(100);
          ESP.restart();
        }
        line = "";
      } else if (line.length() < 512) {
        line += c;
      }
    }
    delay(5);
  }
}

}  // namespace cydstudio::serial_provision
