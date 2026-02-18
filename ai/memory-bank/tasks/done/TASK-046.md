# TASK-046: E2E signed OTA verification on physical device

**Status**: MERGED DONE (2026-02-18, Eero)
**Priority**: P1
**Owner**: Eero
**Size**: S (2 points)

## Summary
Full end-to-end verification of the OTA signing pipeline on physical hardware.
Three tests: signed OTA (accept), tampered binary (reject with SIG_ERR), unsigned
fallback (accept, backward compatible). All passed.

## Issues Found & Fixed
1. Lambda IAM permissions for device registry (terraform)
2. ED25519 verify buffer overflow: 4KB → 16KB (`OTA_VERIFY_BUF_SIZE`)
3. Flash script relative path resolving to stale build
4. Hardcoded version log string in app_tx.c

## Deliverables
- `tests/e2e/RESULTS-task046-signed-ota.md`
- `app/rak4631_evse_monitor/include/ota_update.h`: OTA_VERIFY_BUF_SIZE 16384
- `app/rak4631_evse_monitor/src/ota_update.c`: Enlarged verify buffer
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c`: Fixed version log
- `aws/terraform/main.tf`: Device registry IAM + env vars
