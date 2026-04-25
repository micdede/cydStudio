/**
 * Provisioning — load/save WiFi+hostname+OTA from NVS, and the first-boot
 * SoftAP that lets the Mac app push the credentials in.
 *
 * NVS keys (namespace "cydsetup"):
 *   ssid, psk, host, ota   — strings
 *
 * SoftAP wire protocol:
 *   POST http://<ap-ip>/provision
 *   Content-Type: application/json
 *   { "ssid": "...", "psk": "...", "hostname": "...", "ota_pw": "..." }
 *
 * Compile-time fallback (useful when migrating an existing device that's
 * already on a known WiFi — avoids the SoftAP step):
 *   #define CYDSTUDIO_DEFAULT_WIFI_SSID  "..."
 *   #define CYDSTUDIO_DEFAULT_WIFI_PSK   "..."
 *   #define CYDSTUDIO_DEFAULT_HOSTNAME   "..."
 *   #define CYDSTUDIO_DEFAULT_OTA_PW     "..."
 * If NVS is empty AND these are set, load() returns them as if persisted.
 */
#include "provisioning.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#endif

namespace cydstudio::provisioning {

static const char* NVS_NS = "cydsetup";

Config load() {
  Preferences p;
  p.begin(NVS_NS, /*readOnly=*/true);
  Config c;
  c.ssid         = p.getString("ssid", "");
  c.psk          = p.getString("psk", "");
  c.hostname     = p.getString("host", "");
  c.ota_password = p.getString("ota", "");
  p.end();

#if defined(CYDSTUDIO_DEFAULT_WIFI_SSID) && defined(CYDSTUDIO_DEFAULT_HOSTNAME)
  if (c.ssid.length() == 0) {
    c.ssid     = CYDSTUDIO_DEFAULT_WIFI_SSID;
   #ifdef CYDSTUDIO_DEFAULT_WIFI_PSK
    c.psk      = CYDSTUDIO_DEFAULT_WIFI_PSK;
   #endif
    c.hostname = CYDSTUDIO_DEFAULT_HOSTNAME;
   #ifdef CYDSTUDIO_DEFAULT_OTA_PW
    c.ota_password = CYDSTUDIO_DEFAULT_OTA_PW;
   #endif
    Serial.println("[prov] using compile-time fallback creds");
  }
#endif

  c.valid = c.ssid.length() > 0 && c.hostname.length() > 0;
  return c;
}

void save(const Config& c) {
  Preferences p;
  p.begin(NVS_NS, /*readOnly=*/false);
  p.putString("ssid", c.ssid);
  p.putString("psk",  c.psk);
  p.putString("host", c.hostname);
  p.putString("ota",  c.ota_password);
  p.end();
}

void erase() {
  Preferences p;
  p.begin(NVS_NS, /*readOnly=*/false);
  p.clear();
  p.end();
}

bool run_first_boot_ap(const String& ap_ssid_suffix) {
  WiFi.mode(WIFI_AP);
  String ap_name = String("cydStudio-") + ap_ssid_suffix;
  WiFi.softAP(ap_name.c_str());
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[prov] SoftAP '%s' → http://%s/provision\n", ap_name.c_str(), ip.toString().c_str());

  WebServer server(80);
  bool received = false;
  Config staged;

  server.on("/provision", HTTP_POST, [&] {
    JsonDocument doc;
    auto err = deserializeJson(doc, server.arg("plain"));
    if (err) { server.send(400, "text/plain", err.c_str()); return; }
    if (!doc["ssid"].is<const char*>() || !doc["hostname"].is<const char*>()) {
      server.send(400, "text/plain", "ssid + hostname required");
      return;
    }
    staged.ssid         = doc["ssid"].as<const char*>();
    staged.psk          = doc["psk"]     | "";
    staged.hostname     = doc["hostname"].as<const char*>();
    staged.ota_password = doc["ota_pw"]  | "";
    server.send(200, "application/json", R"({"ok":true,"reboot":true})");
    received = true;
  });

  server.on("/", HTTP_GET, [&] {
    String html = "<!doctype html><meta charset=utf-8><title>cydStudio setup</title>"
                  "<h1>cydStudio</h1><p>POST credentials to /provision.</p>";
    server.send(200, "text/html", html);
  });

  server.begin();

  while (!received) {
    server.handleClient();
    delay(5);
  }

  delay(200);  // let the response flush
  save(staged);
  Serial.println("[prov] saved — restarting");
  return true;
}

}  // namespace cydstudio::provisioning
