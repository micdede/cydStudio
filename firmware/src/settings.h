/**
 * Cross-cutting persisted settings (rotation, brightness, etc.).
 * Stored in NVS namespace "cydsettings" so they survive layout pushes
 * and provisioning resets.
 */
#pragma once
#include <stdint.h>

namespace cydstudio::settings {

int  get_rotation();              // 0..3, default 0
void set_rotation(int quarter);   // persists; takes effect after restart

}  // namespace cydstudio::settings
