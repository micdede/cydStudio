# Adding a board

A board profile is one header file in `firmware/src/boards/` plus an entry in `platformio.ini`. The renderer code never changes.

## What you need to know about the board

- MCU family: ESP32, ESP32-S3, ESP32-C3, etc.
- Display controller chip (ILI9341, ST7789, GC9107, RM67162, …)
- Display resolution and orientation
- SPI or QSPI? Pin assignments (MISO, MOSI, SCLK, CS, DC, RST, BL)
- Backlight: PWM pin and active-high vs active-low (or none, for AMOLED)
- Touch controller (XPT2046, GT911, FT6336, …) and its pins, if present
- USB-Serial chip on the board (CH340, CP210x, native USB-OTG) — informs the flasher's port detection but not the firmware

## Step 1 — Create `firmware/src/boards/<board_id>.h`

Use an existing one as a template. The header must define:

```cpp
#pragma once
#include <LovyanGFX.hpp>

class Board_<id> : public lgfx::LGFX_Device {
  // … bus + panel setup in the constructor …
};

#define BOARD_W 240
#define BOARD_H 320
#define BOARD_HAS_TOUCH 0
#define BOARD_BL_PWM_FREQ 5000
```

## Step 2 — Add a PlatformIO env

In `firmware/platformio.ini`:

```ini
[env:<board_id>]
extends = env:_esp32_base    ; or _esp32s3_base
build_flags =
  ${env:_esp32_base.build_flags}
  -D CYDSTUDIO_BOARD_<ID>
```

The shared `_esp32_base` / `_esp32s3_base` env handles toolchain, partition table, libs.

## Step 3 — Wire it into the dispatcher

`firmware/src/boards/dispatch.h` has a chain of `#if defined(CYDSTUDIO_BOARD_*)` that picks the right `Board_*` type as `BoardDisplay`. Add a clause for your board.

## Step 4 — Test pattern

Build and flash. The first thing the firmware does (before WiFi, before reading NVS) is run a 2-second test pattern: a colored gradient, the board ID, and the resolution. If the pattern looks right, your pin/driver config is correct.

## Step 5 — Add to the Mac app's board picker

In `mac-app/Sources/Flasher/BoardCatalog.swift`, add an entry with the board ID, friendly name, expected USB-Serial chip, and path to the prebuilt firmware image in `Resources/firmware-images/`.

## CI builds the image

When you push your board profile, GitHub Actions builds the corresponding firmware `.bin` and attaches it to the next release. The Mac app downloads images on first launch and caches them.
