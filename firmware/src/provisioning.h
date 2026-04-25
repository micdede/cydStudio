#pragma once
#include <Arduino.h>

namespace cydstudio::provisioning {

struct Config {
  String ssid;
  String psk;
  String hostname;     // mDNS name without .local
  String ota_password;
  bool   valid = false;
};

// Load WiFi/hostname/OTA config from NVS. Returns valid=false if not yet provisioned.
Config load();

// Persist a freshly-provisioned config to NVS.
void save(const Config& cfg);

// Wipe all provisioning state — used by the "factory reset" path.
void erase();

// Block on first-boot SoftAP that exposes POST /provision { ssid, psk, hostname, ota_pw }.
// On success, persists the config and returns true; caller should ESP.restart().
bool run_first_boot_ap(const String& ap_ssid_suffix);

}  // namespace cydstudio::provisioning
