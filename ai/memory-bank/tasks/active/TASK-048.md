# TASK-048: On-device verification — commissioning self-test

**Status**: committed (all acceptance criteria verified end-to-end)
**Priority**: P0
**Owner**: Eero
**Branch**: `task/048-selftest-verify-stale-flash`
**Size**: S (2 points)

## Description
TASK-039 implemented all self-test logic (boot checks, continuous monitoring, `sid selftest` shell command) with 23 C tests and 4 Lambda decoder tests — all passing and merged to main. Two Definition of Done criteria remain that require the physical device:

1. `sid selftest` verified on physical device — Flash current main (platform + app), run `sid selftest` via serial, confirm pass/fail output.
2. Fault flags appear in DynamoDB uplinks — Trigger a fault condition (e.g., simulate clamp mismatch via `evse c` with no load), confirm uplink byte 7 bits 4-7 are set, verify decode Lambda extracts fault fields.

## Dependencies
**Blocked by**: TASK-039 (DONE)
**Blocks**: none

## Acceptance Criteria
- [x] `sid selftest` shell command runs on device and prints pass/fail for each check
- [x] Boot self-test runs on power-on (confirm via serial log)
- [x] Fault flags appear in uplink byte 7 bits 4-7 when a fault condition is active
- [x] DynamoDB event contains decoded fault fields (`sensor_fault`, `clamp_mismatch`, `interlock_fault`, `selftest_fail`)
- [x] Fault flags clear when condition resolves (interlock cleared via `app evse allow`, verified in DynamoDB)

## Bugs Found and Fixed During Verification
1. **GPIO readback** — `GPIO_OUTPUT_ACTIVE` without `GPIO_INPUT` disconnects nRF52840 input buffer → selftest charge_en FAIL. Fixed in platform_api_impl.c.
2. **BSS uninitialized** — Split-image architecture has no C runtime BSS init → fault_flags starts at 0xFF garbage. Fixed: platform zeroes APP_RAM, app calls selftest_reset().
3. **LED out of range** — selftest uses LED index 2, RAK4631 only has 0-1. Cosmetic, not fixed.

## Deliverables
- `tests/e2e/RESULTS-selftest.md` — filled with device test results
- Updated `tests/e2e/RUNBOOK.md` — §6 and §7 added
- Fixed `platform_api_impl.c` — GPIO_INPUT flag
- Fixed `app.c` — APP_RAM zeroing
- Fixed `app_entry.c` — selftest_reset() before selftest_boot()
- Added `platform_api.h` — APP_RAM_ADDR/SIZE constants
