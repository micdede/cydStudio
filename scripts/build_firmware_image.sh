#!/usr/bin/env bash
# Build a single merged firmware image for one board and copy it into the
# mac-app's bundled-resources directory. The Mac app's Flasher writes this
# image with one esptool invocation at offset 0x0.
#
# Usage:  scripts/build_firmware_image.sh [board-env]
# Default board: cyd-2432s028
set -euo pipefail

BOARD="${1:-cyd-2432s028}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW_DIR="$ROOT/firmware"
PIO_BUILD="$FW_DIR/.pio/build/$BOARD"
OUT_DIR="$ROOT/mac-app/Sources/CydStudio/Resources/firmware"
OUT_BIN="$OUT_DIR/$BOARD.bin"

PIO="${PIO:-/Library/Frameworks/Python.framework/Versions/3.13/bin/pio}"
PYTHON3="${PYTHON3:-/Library/Frameworks/Python.framework/Versions/3.13/bin/python3}"
ESPTOOL="${ESPTOOL:-$HOME/.platformio/packages/tool-esptoolpy/esptool.py}"
BOOT_APP0="${BOOT_APP0:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"

echo "[image] building $BOARD"
"$PIO" run -e "$BOARD" -d "$FW_DIR"

if [ ! -f "$BOOT_APP0" ]; then
  echo "[image] boot_app0.bin not found at $BOOT_APP0" >&2
  echo "        export BOOT_APP0=/path/to/boot_app0.bin and re-run" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
echo "[image] merging into $OUT_BIN"
"$PYTHON3" "$ESPTOOL" --chip esp32 merge_bin \
  -o "$OUT_BIN" \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000  "$PIO_BUILD/bootloader.bin" \
  0x8000  "$PIO_BUILD/partitions.bin" \
  0xe000  "$BOOT_APP0" \
  0x10000 "$PIO_BUILD/firmware.bin"

ls -lh "$OUT_BIN"
