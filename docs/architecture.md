# Architecture

## TL;DR

The ESP32 is a thin, dumb, fast renderer. The Mac app is the brain. The LLM is an optional design assistant that the Mac app calls — never the ESP.

```
                          ┌───────────────────────┐
                          │    macOS app           │
                          │  (cydStudio)           │
                          │                       │
                          │  Flasher · Designer  │
                          │  AI-Bridge · mDNS    │
                          └───────────────────────┘
                                  │       ▲
       ┌──────────────────────────┘       │
       │ HTTP/JSON                       │ HTTPS
       │ (push layout, query state)     │
       ▼                                 │
┌──────────────────┐               ┌─────┴──────────────┐
│  ESP32 device   │               │  LLM provider     │
│                 │               │  (Anthropic /     │
│  LovyanGFX     │               │   OpenAI-compat / │
│  + LVGL        │  HTTP polls   │   local Ollama)   │
│  + Renderer    │  ───────────▶ └────────────────────┘
│  + Datasources │
└──────────────────┘
       │ HTTP polls
       ▼
┌──────────────────┐
│ External APIs   │
│ (weather, RSS,  │
│  smart home,    │
│  Claude usage)  │
└──────────────────┘
```

## Components

### macOS app (`mac-app/`)

Built with SwiftUI, targets macOS 14+. Five sub-systems:

- **Flasher** — wraps a bundled `esptool` binary. Detects USB-Serial ports (CH340, CP210x, ESP32-S3 native USB), shows a board picker, flashes the right firmware image, monitors progress.
- **Provisioning** — connects to the device's first-boot AP (`cydStudio-XXXX`), POSTs WiFi credentials + hostname + OTA password, waits for the device to reboot and announce itself via mDNS.
- **DeviceRegistry** — discovers `_cydstudio._tcp` Bonjour services, tracks online/offline, stores per-device metadata (board profile, current layout hash).
- **Designer** — JSON editor (Phase 2) → drag-and-drop canvas (Phase 6). Live preview at the device's native resolution. Push-on-save.
- **AI** — provider-agnostic interface: prompt + current layout + schema → new layout. API keys live in the macOS Keychain, never in UserDefaults.

### ESP32 firmware (`firmware/`)

PlatformIO project, one env per supported board. Three layers:

- **Board HAL** (`src/boards/`) — pin definitions and a `make_display()` factory. New boards drop a header here.
- **Display stack** — LovyanGFX as low-level driver (handles ILI9341, ST7789, GC9107, RM67162 QSPI behind one API), LVGL 8.3 on top for widget rendering.
- **Application** — `provisioning.cpp` (first-boot AP + NVS), `renderer.cpp` (JSON → LVGL widget tree), `datasources.cpp` (HTTP polling, push, internal sources like clock/heap), `widgets/` (one file per supported widget type).

### Schemas (`schema/`)

`layout.schema.json` is the contract. The Mac app validates outbound layouts against it. The LLM is given the schema as part of its prompt. The firmware can be conservative on input — anything that doesn't match a known widget type is logged and skipped, but a malformed layout never bricks the device (the previous good layout stays in NVS).

## Data flow: first boot

1. Power on, no WiFi creds in NVS → boot into AP mode (`cydStudio-XXXX`, open).
2. Display shows: SSID name, a QR code that opens the Mac app's provisioning URL, and a help line.
3. Mac app connects to the AP, POSTs `/provision { ssid, psk, hostname, ota_pw }`.
4. ESP saves to NVS, replies 200, reboots into station mode.
5. ESP joins WiFi, starts mDNS as `<hostname>._cydstudio._tcp`.
6. Mac app sees the new device pop up in DeviceRegistry, offers to push a starter layout.

## Data flow: layout update

1. User edits in Designer, hits Push (or AI returns a layout).
2. Mac app validates against `layout.schema.json`. Bad → show errors, don't push.
3. App POSTs `/layout` with the JSON body. ESP atomically writes to NVS, swaps the in-memory layout, kicks off any new datasource pollers.
4. ESP responds with the layout's content hash. Mac app stores it for diff display.

## Data flow: runtime values

ESP polls each datasource on its own schedule. For each `widget` with a `binding`, the renderer subscribes to that path; when the value changes, the widget redraws. Datasources can also be **push-only** (Mac or external service POSTs `/data { source_id, payload }`) — useful for the original Claude Code statusline integration.

## Why not put the LLM on the ESP?

- API keys on a multi-device flashed firmware = security disaster
- LLM responses take seconds; UI needs to stay responsive
- Designer needs the LLM output to validate/edit/push, so the Mac is in the loop anyway

## Why LovyanGFX (not TFT_eSPI)?

LovyanGFX supports every display in our target list including the AMOLED RM67162 over QSPI. TFT_eSPI doesn't. Using one driver across all boards keeps the renderer code single-path.

## Why HTTP/JSON (not MQTT, not custom binary)?

- The Mac app's design loop is human-paced (seconds, not ms) — no protocol overhead matters
- Curl-debuggable from anywhere
- LLMs natively produce JSON; no encoding step needed
- mDNS + HTTP is well-supported in SwiftUI's URLSession + NWBrowser

## Out of scope (for now)

- Cloud sync of layouts between users
- Marketplace / shared layout repository
- Mobile companion app
- Audio / speakers
- Battery management for portable boards
