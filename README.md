# cydStudio

Build screens for cheap ESP32-based displays — without writing C.

cydStudio is two things working together:

1. A **macOS app** that flashes generic firmware onto an ESP32 display board, gets it onto your WiFi, and lets you design what it shows. You can drive the design with an LLM (Anthropic, OpenAI-compatible, or local Ollama) — describe what you want, get a layout pushed to the device.
2. A **generic ESP32 firmware** that boots into a setup AP on first power-on, joins your WiFi, and then renders any layout you push to it as JSON. No re-flash needed when you change your mind about what should be on the screen.

The display becomes a thin client. The Mac app and (optionally) an LLM are the brain.

## Status

Pre-alpha. Bootstrapping the repo. The original [Claude Code Status Monitor](https://github.com/) (this repo's predecessor) is being migrated as the first example layout.

## Supported boards

| Board | Chip | Display | Touch |
|---|---|---|---|
| ESP32-2432S028 (CYD) | ESP32 | 240×320 ILI9341 | — |
| ESP32-2432S028R (CYD-R) | ESP32 | 240×320 ILI9341 | XPT2046 resistive |
| LILYGO T-Display | ESP32 | 135×240 ST7789 | — |
| LILYGO T-Display S3 | ESP32-S3 | 170×320 ST7789 | — |
| LILYGO T-QT Pro | ESP32-S3 | 128×128 GC9107 | — |
| LILYGO T-Display S3 AMOLED | ESP32-S3 | 536×240 RM67162 (QSPI) | — |

Adding a board is a matter of dropping a pin/driver profile into `firmware/src/boards/`. See [docs/adding-a-board.md](docs/adding-a-board.md).

## Architecture (one paragraph)

The Mac app talks to the ESP32 over HTTP via mDNS (`<hostname>.local`). It pushes a **layout JSON** describing widgets and **datasource definitions** describing where to fetch values. The ESP32 polls those datasources itself, so the screen keeps working when your Mac is asleep. The LLM never talks to the ESP — only the Mac app calls it, validates the result against [schema/layout.schema.json](schema/layout.schema.json), and pushes the validated layout. Full diagram: [docs/architecture.md](docs/architecture.md).

## Repo layout

```
mac-app/        SwiftUI macOS app — flasher, provisioning, designer, AI-bridge
firmware/       generic ESP32 renderer (PlatformIO, LovyanGFX + LVGL)
schema/         layout.schema.json — the contract between app, AI, and firmware
docs/           architecture, adding-a-board, schema reference
scripts/        helper Python (asset conversion, schema validation, etc.)
```

## Roadmap

- **Phase 1** — Generic firmware on the CYD; `/layout` HTTP endpoint; renders the migrated Claude monitor from JSON
- **Phase 2** — Mac app skeleton; mDNS discovery; layout-push from a textual JSON editor
- **Phase 3** — Flasher + first-boot AP + provisioning UX (no terminal needed)
- **Phase 4** — AI integration: prompt-to-layout with schema validation
- **Phase 5** — Second board (T-Display S3) to prove the abstractions
- **Phase 6** — Drag-and-drop visual designer

## Building from source

Until Phase 3 ships a signed app bundle, you'll need:

- macOS 14+ (Sonoma) and Xcode 15+
- PlatformIO (`pip3 install -U platformio`)
- A USB-Serial cable for the first flash; OTA after that

```bash
# Build firmware for your board
cd firmware
pio run -e cyd-2432s028

# Open the Mac app (Phase 2+)
open mac-app/CydStudio.xcodeproj
```

## License

MIT — see [LICENSE](LICENSE).
