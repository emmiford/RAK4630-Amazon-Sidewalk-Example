# TASK-053: Resolve two app_tx.c naming collision

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
Two files named `app_tx.c` exist in different directories doing different things:

- `src/app_tx.c` (platform, 50 lines) — TX readiness state holder + empty `send_evse_data()` stub
- `src/app_evse/app_tx.c` (app, 113 lines) — actual 12-byte uplink encoding

The platform-side file is only meaningful in "platform-only" mode (no app image loaded). If this mode is not a real use case, delete the platform's `app_tx.c` entirely and inline the 2 state variables into `platform_api_impl.c`.

If platform-only mode is needed, rename `src/app_tx.c` → `src/tx_state.c` (and header to `tx_state.h`).

Reference: `docs/technical-design-rak-firmware.md`, Change 4.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] No two files with the same base name doing different things
- [ ] Platform build succeeds
- [ ] App build succeeds
- [ ] Host tests pass
- [ ] `app_tx.h` (platform) renamed or removed; no ambiguity with app's `app_tx.h`

## Testing Requirements
- [ ] Platform build succeeds
- [ ] App build succeeds
- [ ] 57 host-side tests pass

## Deliverables
- 1 file renamed or deleted, corresponding header updated
- References updated across platform sources
