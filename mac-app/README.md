# CydStudio (macOS app)

SwiftUI-based controller for cydStudio devices on your local network.

Phase 2+3+4 scope (this version):

- **Device discovery** via Bonjour (`_cydstudio._tcp`) — all reachable devices
  show up in the sidebar automatically as soon as the firmware joins WiFi
- **JSON layout editor** with example loader (the `examples/` from this repo
  are bundled into the app)
- **Push** the current editor contents to the selected device via `POST /layout`
- **Status banner** with the last-push outcome and any error returned by the device
- **Add Device** flow: pick USB port → flash bundled firmware via esptool →
  enter WiFi credentials → app sends them over the same USB cable, the
  device joins your network and appears in the sidebar
- **AI prompt-to-layout**: describe what you want in plain language; the
  active provider returns a JSON layout that drops straight into the editor
  (Anthropic / OpenAI-compatible / local Ollama; pick in the AI Provider sheet)

Future phases will add: drag-and-drop designer, AI prompt-to-layout,
multi-page layouts, bundled esptool binary so no Python install is needed.

## Adding a new device (Phase 3)

1. Plug your CYD into USB. Wait a couple of seconds for macOS to mount the
   USB-Serial device (you'll see `/dev/cu.usbserial-XXXX`).
2. Click the `+` button in the sidebar to open the **Add Device** sheet.
3. Pick the serial port and the board profile (only `cyd-2432s028` is
   shipped right now; more profiles arrive in Phase 5).
4. Click **Flash**. You'll see esptool's progress bar.
5. After the flash, fill in WiFi SSID + password + a hostname (e.g.
   `kitchen-monitor`) and an optional OTA password.
6. Click **Send**. The app writes the credentials over the same USB cable,
   the device persists them and reboots, joins your network, and appears
   in the sidebar.

### Prerequisites

- `esptool.py` reachable. The app looks under `~/.platformio/packages/tool-esptoolpy/`
  first (you have it if PlatformIO is installed) and falls back to PATH.
  If you don't have either, install with:
  ```
  pip3 install esptool
  ```
- A bundled firmware image at
  `Sources/CydStudio/Resources/firmware/<board>.bin`. Re-generate via:
  ```
  scripts/build_firmware_image.sh cyd-2432s028
  ```

## AI prompt-to-layout (Phase 4)

Click the sparkle icon in the toolbar to open **AI Provider** settings:

- **Anthropic** — the default. Set your API key from
  https://console.anthropic.com. Default model: `claude-opus-4-7`.
- **OpenAI-compatible** — works against the real OpenAI API or anything that
  speaks the same wire format (OpenRouter, Groq, Together, LM Studio).
  Set base URL + key + model.
- **Ollama** — local server, no key needed. Default `http://localhost:11434`,
  default model `llama3.2`. Smaller local models will struggle to produce
  schema-correct JSON; an 8B+ model is the practical floor.

API keys live in the macOS Keychain (`com.dedecke.cydstudio.aikeys`),
not in UserDefaults.

Once configured, type a prompt in the Designer's prompt bar
("Wetterstation für Berlin", "BTC price + RSI gauge", etc.) and click
**Generate**. The app sends the layout schema + a reference example +
your prompt, gets back JSON, validates it parses, and drops it into the
editor. Click **Push layout** to send it to the device.

## Run from source

```bash
cd mac-app
swift run
```

The first build pulls dependencies and takes ~15s. Subsequent runs are fast.

To open in Xcode for proper SwiftUI previews and debugger:

```bash
open Package.swift
```

## Requirements

- macOS 14 (Sonoma) or later
- Xcode 15 or later (or just the command-line tools — `swift run` works without
  a full Xcode install)

## Architecture

```
Sources/CydStudio/
├── App.swift                     @main + root NavigationSplitView
├── Model/
│   └── Device.swift              Observable device record
├── DeviceList/
│   ├── DeviceRegistry.swift      NWBrowser for _cydstudio._tcp
│   └── DeviceListView.swift      Sidebar
├── Designer/
│   ├── DesignerView.swift        JSON editor + push button + status bar
│   └── DeviceClient.swift        Async URLSession wrapper for /health, /layout, /data/{id}
└── Resources/
    ├── examples/                 Bundled layout examples (mirrors /schema/examples)
    └── schema/                   Bundled JSON schemas (used by AI prompt in Phase 4)
```

## Known limitations

- No code signing yet — Gatekeeper will warn on first launch when the app is
  shared as a `.app` bundle. SPM-built binaries running locally are fine.
- `TextEditor` has no JSON syntax highlighting yet (Phase 2 ships with the
  default monospaced font; rich highlighting comes in Phase 6 with the
  visual designer).
- No persistence of in-progress edits — close-without-push loses changes.
