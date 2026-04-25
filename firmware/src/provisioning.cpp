/**
 * Provisioning — load/save WiFi+hostname+OTA from NVS, and the first-boot
 * SoftAP that lets the Mac app push the credentials in.
 *
 * Wire format for POST /provision (sent by mac-app/Sources/Provisioning):
 *   { "ssid": "...", "psk": "...", "hostname": "...", "ota_pw": "..." }
 *
 * STATUS: stubbed for repo bootstrap. Real implementation in Phase 1.
 */
#include "provisioning.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

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
  c.valid        = c.ssid.length() > 0 && c.hostname.length() > 0;
  p.end();
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
  // TODO Phase 1: bring up SoftAP, expose POST /provision, persist, return true.
  // Stub blocks forever so it's obvious we entered this path during bring-up.
  WiFi.mode(WIFI_AP);
  String ap_name = String("cydStudio-") + ap_ssid_suffix;
  WiFi.softAP(ap_name.c_str());
  Serial.printf("[prov] SoftAP up as '%s' — implement /provision route\n", ap_name.c_str());
  while (true) { delay(1000); }
}

}  // namespace cydstudio::provisioning
