#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

namespace cydstudio::datasources {

// Set up datasources from a layout's "datasources" array. Tears down any
// existing pollers first. Idempotent.
void configure(JsonArrayConst sources);

// Resolve a binding path like "weather.temp_c" against the current value tree.
// Writes the value into `out` if found and not stale; returns false otherwise.
bool resolve(const String& path, JsonVariant& out);

// Drive the HTTP polling loop. Call from loop().
void tick();

// Push handler — called by the HTTP server for POST /data/{id}.
// Returns "" on success, error string on failure.
String accept_push(const String& source_id, const JsonDocument& body);

// Subscribe to changes on a binding path. The callback fires after every
// successful fetch/push that touches the path. Used by the renderer to
// invalidate widgets.
using ChangeCb = std::function<void(const String& path)>;
void on_change(ChangeCb cb);

// Called by the renderer when a new page is displayed. Starts the __page.pct
// counter (0→100 over duration_ms). Pass 0 to stop the counter.
void page_started(uint32_t duration_ms);

}  // namespace cydstudio::datasources
