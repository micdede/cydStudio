# CydStudio (macOS app)

SwiftUI-based controller for cydStudio devices on your local network.

Phase 2 scope (this version):

- **Device discovery** via Bonjour (`_cydstudio._tcp`) — all reachable devices
  show up in the sidebar automatically as soon as the firmware joins WiFi
- **JSON layout editor** with example loader (the `examples/` from this repo
  are bundled into the app)
- **Push** the current editor contents to the selected device via `POST /layout`
- **Status banner** with the last-push outcome and any error returned by the device

Future phases will add: drag-and-drop designer, AI prompt-to-layout,
flasher / first-boot provisioning, multi-page layouts.

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
