#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace cydstudio::format {

/**
 * Substitute placeholders in `tmpl` with the given value.
 *
 * Supported tokens:
 *   {}              raw value (number → cleanest representation, string → as-is)
 *   {:.0f} {:.1f}   float with N decimals
 *   {:d}            integer
 *   {duration}      seconds → "1h 23m" or "23m 45s" or "45s"
 *
 * Multiple tokens in one template are not supported in v0.1 (single binding
 * per widget). Returns the substituted string.
 */
String apply(const String& tmpl, JsonVariantConst value);

/// Standalone seconds → human duration helper, exposed for tests.
String duration(int64_t seconds);

}  // namespace cydstudio::format
