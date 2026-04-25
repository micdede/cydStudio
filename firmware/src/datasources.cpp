/**
 * Datasources — registry of HTTP-poll, push, and internal value sources.
 *
 * STATUS: stubbed. Phase 1 work:
 *   - HTTP poller with per-source poll_sec, JSONPath extraction (use ArduinoJson),
 *     stale tracking
 *   - push source: enforce field whitelist, stale-after-sec, last-update timestamp
 *   - internal sources: clock (with TZ), uptime, heap, wifi RSSI, board info
 *   - resolve("a.b.c") → walk an internal JsonDocument
 */
#include "datasources.h"

#include <vector>

namespace cydstudio::datasources {

static std::vector<ChangeCb> change_callbacks;
static JsonDocument           value_tree;

void configure(JsonArrayConst sources) {
  // TODO Phase 1: instantiate pollers, reset value_tree, register push handlers.
  (void)sources;
}

bool resolve(const String& path, JsonVariant& out) {
  // TODO Phase 1: walk dotted path through value_tree.
  (void)path;
  (void)out;
  return false;
}

void tick() {
  // TODO Phase 1: drive HTTP polling, internal source ticking.
}

String accept_push(const String& source_id, const JsonDocument& body) {
  // TODO Phase 1: validate against fields whitelist, merge into value_tree,
  // fire change callbacks for each touched path.
  (void)source_id;
  (void)body;
  return "";
}

void on_change(ChangeCb cb) {
  change_callbacks.push_back(std::move(cb));
}

}  // namespace cydstudio::datasources
