#pragma once
#include <stdint.h>

namespace cydstudio::serial_provision {

/**
 * Listen on Serial for a one-shot provisioning command from a host tool
 * (the cydStudio Mac app uses this right after flashing over USB).
 *
 * Wire protocol — emitted by firmware:
 *   "CYDSTUDIO_PROVISION_READY\n"
 *
 * Wire protocol — accepted from host (single line, terminated by \n):
 *   PROVISION {"ssid":"...","psk":"...","hostname":"...","ota_pw":"..."}
 *
 * On a valid PROVISION line we persist via provisioning::save and reboot.
 * If nothing arrives within `timeout_ms`, we return and let normal boot
 * continue (NVS / SoftAP fallback).
 */
void run(uint32_t timeout_ms);

}  // namespace cydstudio::serial_provision
