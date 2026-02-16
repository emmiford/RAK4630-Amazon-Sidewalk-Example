# TASK-053: Resolve two app_tx.c naming collision

**Status**: in progress (2026-02-15, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: `task/053-tx-state-rename`
**Size**: S (1 point)

## Description
Two files named `app_tx.c` exist in different directories doing different things:

- `src/app_tx.c` (platform, 50 lines) — TX readiness state holder + empty `send_evse_data()` stub
- `src/app_evse/app_tx.c` (app, 113 lines) — actual 12-byte uplink encoding

Chose Option A (rename): platform-only mode is a real use case (device boots without app image).

Renamed `src/app_tx.c` → `src/tx_state.c` with new header `include/tx_state.h`.
All platform functions renamed from `app_tx_*` → `tx_state_*`.

Reference: `docs/technical-design-rak-firmware.md`, Change 4.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] No two files with the same base name doing different things
- [x] Platform build succeeds (CMakeLists.txt updated)
- [x] App build succeeds (app-side untouched)
- [x] Host tests pass (55/55 Makefile + 13/13 CMake)
- [x] `app_tx.h` (platform) renamed or removed; no ambiguity with app's `app_tx.h`

## Testing Requirements
- [x] Platform build succeeds (CMakeLists.txt references tx_state.c)
- [x] App build succeeds (app_evse/app_tx.c untouched)
- [x] Host-side tests pass (55/55 + 13/13 = all green)

## Deliverables
- Deleted `src/app_tx.c`, created `src/tx_state.c` (renamed functions)
- Created `include/tx_state.h` (new platform-side header)
- Updated `app.c`, `platform_api_impl.c`, `sid_shell.c` — include + call sites
- Updated `CMakeLists.txt` — `src/tx_state.c`
- App-side `app_tx.c` and `app_tx.h` unchanged
