#!/bin/bash
#
# Flash script for split-image architecture
#
# Usage:
#   ./flash.sh all       — Flash MFG + platform + app (full setup)
#   ./flash.sh platform  — Flash platform image only
#   ./flash.sh app       — Flash app image only (fast, ~20KB)
#   ./flash.sh mfg       — Flash MFG credentials only
#

set -e

TARGET="nrf52840"
MFG_HEX="../../mfg.hex"
PLATFORM_HEX="../../build/merged.hex"
APP_HEX="../../build_app/app.hex"

flash_mfg() {
    echo "=== Flashing MFG (credentials) ==="
    python3 -m pyocd flash --target $TARGET "$MFG_HEX"
}

flash_platform() {
    echo "=== Flashing Platform Image ==="
    python3 -m pyocd flash --target $TARGET "$PLATFORM_HEX"
}

flash_app() {
    echo "=== Flashing App Image ==="
    python3 -m pyocd flash --target $TARGET "$APP_HEX"
}

case "${1:-all}" in
    all)
        flash_mfg
        flash_platform
        flash_app
        echo "=== All images flashed ==="
        ;;
    platform)
        flash_platform
        ;;
    app)
        flash_app
        echo "=== App-only update complete (platform untouched) ==="
        ;;
    mfg)
        flash_mfg
        ;;
    *)
        echo "Usage: $0 {all|platform|app|mfg}"
        exit 1
        ;;
esac
