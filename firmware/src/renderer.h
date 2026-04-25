#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <lvgl.h>

namespace cydstudio::renderer {

// Replace the active layout with the given JSON document. Validates against
// the embedded schema; on failure, returns an error string and leaves the
// previous layout running. On success, returns "".
String apply_layout(const JsonDocument& doc);

// Persist the currently active layout to NVS. Called after a successful apply.
void persist_active();

// Try to load the last-good layout from NVS at boot. Returns true if a layout
// was applied; false if NVS is empty (caller should show a "waiting for layout"
// placeholder screen).
bool load_persisted();

// LVGL teardown — used when the firmware switches into OTA mode and we want
// to free DMA buffers.
void shutdown();

// True if a layout push asked for a setting that requires a reboot (e.g. a
// rotation change). main.cpp checks this each loop and calls ESP.restart()
// once enough time has passed for the HTTP response to flush.
bool pending_restart();

}  // namespace cydstudio::renderer
