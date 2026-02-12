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

warn_huk() {
    echo ""
    echo "  WARNING: Platform flash erases the HUK (Hardware Unique Key)."
    echo "  PSA crypto keys will be re-derived on next boot."
    echo "  If MFG credentials are stale, Sidewalk crypto will fail (error -149)."
    echo ""
    echo "  Safe order: ./flash.sh mfg && ./flash.sh platform && ./flash.sh app"
    echo "  Or simply:  ./flash.sh all"
    echo ""
    read -p "  Flash platform without MFG? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Cancelled."
        exit 1
    fi
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
        warn_huk
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
