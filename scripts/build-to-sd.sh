#!/usr/bin/env bash
# Build the firmware and copy firmware.bin to a mounted SD card.
#
# Usage:   scripts/build-to-sd.sh                   # auto-detects /Volumes/<SD>
#          scripts/build-to-sd.sh /Volumes/MYSD     # explicit path
#          scripts/build-to-sd.sh /Volumes/MYSD HP12C.bin
#
# Looks for an SD card mounted under /Volumes that is FAT32 / msdos.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
APP_NAME="${2:-HP12C.bin}"
APPS_DIR="apps"

echo ">> Building firmware..."
"$PIO" run -e cardputer

BIN="$PROJECT_DIR/.pio/build/cardputer/HP12C.bin"
[[ -f "$BIN" ]] || { echo "build output not found: $BIN"; exit 1; }

# Resolve SD mount.
if [[ $# -ge 1 && -d "$1" ]]; then
    SD="$1"
else
    SD=$(diskutil list 2>/dev/null \
        | awk '/FAT32|MS-DOS/ {print $0}' \
        | head -1 \
        | sed -E 's/.* +([^ ]+)$/\1/' || true)
    if [[ -z "${SD:-}" ]]; then
        echo "No FAT32 SD card found under /Volumes. Pass path explicitly:" >&2
        echo "  $0 /Volumes/<your-sd>" >&2
        exit 1
    fi
    SD="/Volumes/$(diskutil info "$SD" | awk -F: '/Volume Name/ {gsub(/^ +/,"",$2); print $2}')"
fi

[[ -d "$SD" ]] || { echo "SD path not a directory: $SD"; exit 1; }
mkdir -p "$SD/$APPS_DIR"
cp -v "$BIN" "$SD/$APPS_DIR/$APP_NAME"
sync
echo ">> Done. Eject the card and run from Launcher → Apps → SD → $APP_NAME"
