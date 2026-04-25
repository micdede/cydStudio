#!/usr/bin/env bash
# Build a single merged firmware image for one board and copy it into the
# mac-app's bundled-resources directory. The Mac app's Flasher writes this
# image with one esptool invocation at offset 0x0.
#
# Usage:  scripts/build_firmware_image.sh [board-env]
# Default board: cyd-2432s028
#
# Pass `--all` to build every supported board:
#   scripts/build_firmware_image.sh --all
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW_DIR="$ROOT/firmware"
OUT_DIR="$ROOT/mac-app/Sources/CydStudio/Resources/firmware"

PIO="${PIO:-/Library/Frameworks/Python.framework/Versions/3.13/bin/pio}"
PYTHON3="${PYTHON3:-/Library/Frameworks/Python.framework/Versions/3.13/bin/python3}"
ESPTOOL="${ESPTOOL:-$HOME/.platformio/packages/tool-esptoolpy/esptool.py}"
BOOT_APP0="${BOOT_APP0:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"

# Map board env name -> esptool --chip arg + bootloader offset.
# (ESP32 puts the bootloader at 0x1000; ESP32-S3 puts it at 0x0.)
chip_for_board() {
  case "$1" in
    cyd-2432s028|cyd-2432s028r|t-display) echo "esp32";;
    t-display-s3|t-qt-pro|t-display-s3-amoled) echo "esp32s3";;
    *) echo "unknown" ;;
  esac
}

bootloader_offset_for_chip() {
  case "$1" in
    esp32)   echo "0x1000";;
    esp32s3) echo "0x0";;
  esac
}

build_one() {
  local BOARD="$1"
  local CHIP=$(chip_for_board "$BOARD")
  if [ "$CHIP" = "unknown" ]; then
    echo "[image] unknown chip family for $BOARD — add a clause in chip_for_board()" >&2
    return 1
  fi
  local BOOT_OFFSET=$(bootloader_offset_for_chip "$CHIP")
  local PIO_BUILD="$FW_DIR/.pio/build/$BOARD"
  local OUT_BIN="$OUT_DIR/$BOARD.bin"

  echo "[image] building $BOARD ($CHIP)"
  "$PIO" run -e "$BOARD" -d "$FW_DIR"

  if [ ! -f "$BOOT_APP0" ]; then
    echo "[image] boot_app0.bin not found at $BOOT_APP0" >&2
    return 1
  fi

  mkdir -p "$OUT_DIR"
  echo "[image] merging into $OUT_BIN"
  "$PYTHON3" "$ESPTOOL" --chip "$CHIP" merge_bin \
    -o "$OUT_BIN" \
    --flash_mode dio --flash_freq 40m --flash_size 4MB \
    "$BOOT_OFFSET" "$PIO_BUILD/bootloader.bin" \
    0x8000  "$PIO_BUILD/partitions.bin" \
    0xe000  "$BOOT_APP0" \
    0x10000 "$PIO_BUILD/firmware.bin"

  ls -lh "$OUT_BIN"
}

main() {
  if [ "${1:-cyd-2432s028}" = "--all" ]; then
    for board in cyd-2432s028 t-display t-display-s3 t-qt-pro; do
      build_one "$board"
    done
  else
    build_one "${1:-cyd-2432s028}"
  fi
}

main "$@"
