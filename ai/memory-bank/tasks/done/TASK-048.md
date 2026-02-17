# TASK-048: On-device selftest verification

**Status**: merged done (2026-02-17)
**Priority**: P0
**Owner**: Eero
**Branch**: `task/048-selftest-verify-stale-flash` (merged to main)

## Summary
Device-verified all selftest acceptance criteria end-to-end. All 5 hardware path checks pass (`sid selftest`). Fault flags verified through full pipeline: device → LoRa → Lambda → DynamoDB. Interlock flag clearing confirmed on device.

Found and fixed 3 bugs during verification:
1. GPIO readback — nRF52840 input buffer disconnected on output-only pins
2. BSS uninitialized — split-image architecture has no C runtime BSS init
3. LED index out of range — cosmetic, not fixed

## Deliverables
- `tests/e2e/RESULTS-selftest.md` — device test results
- `tests/e2e/RUNBOOK.md` — §6 (selftest) and §7 (stale flash) added
- `app/rak4631_evse_monitor/src/platform_api_impl.c` — GPIO_INPUT flag fix
- `app/rak4631_evse_monitor/src/app.c` — APP_RAM zeroing before app init
- `app/rak4631_evse_monitor/src/app_evse/app_entry.c` — selftest_reset() before boot
- `app/rak4631_evse_monitor/include/platform_api.h` — APP_RAM_ADDR/SIZE constants
- `drivers/CMakeLists.txt`, `drivers/Kconfig` — build stubs
