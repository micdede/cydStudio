#include "format.h"

namespace cydstudio::format {

String duration(int64_t seconds) {
  if (seconds < 0) seconds = 0;
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  int s = seconds % 60;
  char buf[16];
  if (h > 0)      snprintf(buf, sizeof(buf), "%dh %02dm", h, m);
  else if (m > 0) snprintf(buf, sizeof(buf), "%dm %02ds", m, s);
  else            snprintf(buf, sizeof(buf), "%ds",       s);
  return String(buf);
}

static String value_to_string(JsonVariantConst v) {
  if (v.is<bool>())   return v.as<bool>() ? "true" : "false";
  if (v.is<int>())    return String(v.as<int>());
  if (v.is<long>())   return String(v.as<long>());
  if (v.is<float>() || v.is<double>()) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%g", v.as<double>());
    return String(buf);
  }
  if (v.is<const char*>()) return String(v.as<const char*>());
  return String("");
}

String apply(const String& tmpl, JsonVariantConst value) {
  int open = tmpl.indexOf('{');
  if (open < 0) return tmpl;
  int close = tmpl.indexOf('}', open);
  if (close < 0) return tmpl;

  String spec = tmpl.substring(open + 1, close);
  String head = tmpl.substring(0, open);
  String tail = tmpl.substring(close + 1);
  String middle;

  if (spec.length() == 0) {
    middle = value_to_string(value);
  } else if (spec == "duration") {
    middle = duration(value.as<int64_t>());
  } else if (spec.startsWith(":")) {
    String fmt = spec.substring(1);  // e.g. ".0f", ".1f", "d"
    char buf[32];
    if (fmt.endsWith("f")) {
      String pfmt = String("%") + fmt;
      snprintf(buf, sizeof(buf), pfmt.c_str(), value.as<double>());
    } else if (fmt == "d") {
      snprintf(buf, sizeof(buf), "%d", value.as<int>());
    } else {
      // unknown spec — fall back to raw
      return head + value_to_string(value) + tail;
    }
    middle = String(buf);
  } else {
    middle = value_to_string(value);
  }

  return head + middle + tail;
}

}  // namespace cydstudio::format
