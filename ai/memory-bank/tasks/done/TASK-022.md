# TASK-022: BUG — Stale flash data inflates OTA delta baselines

**Status**: merged done (2026-02-17)
**Priority**: P1
**Owner**: Eero
**Branch**: `task/048-selftest-verify-stale-flash` (merged to main)

## Summary
Three-layer defense against stale flash data inflating OTA delta baselines. Device-verified: partition dump shows exactly 8444 bytes used, 249604 bytes erased, zero stale bytes.

## Deliverables
- `app/rak4631_evse_monitor/flash.sh` — erase partition before flash, PYOCD variable, pyocd erase --sector
- `app/rak4631_evse_monitor/src/ota_update.c` — erase_stale_app_pages() after OTA apply (3 paths)
- `aws/ota_deploy.py` — baseline size warning if dump >> app binary
- `aws/tests/test_ota_deploy_cli.py` — 5 new tests for baseline warning
