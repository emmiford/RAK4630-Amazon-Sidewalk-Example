# TASK-072: On-device Charge Now button GPIO verification

**Status**: partial pass (2026-02-17, Eero)
**Priority**: P2
**Owner**: Eero
**Branch**: `task/072-button-gpio-verify`
**Size**: XS (1 point)

## Description
TASK-062 wired up the Charge Now button GPIO (P0.07) in the devicetree overlay,
platform_api_impl.c, and TDD. This task covers the on-device manual verification
that the physical button reads correctly and the 5-press selftest trigger fires.

## Dependencies
**Blocked by**: TASK-062 (merged)
**Blocks**: none

## Acceptance Criteria
- [x] `app selftest` shell command shows button GPIO reads 0 when not pressed, 1 when pressed
  - GPIO=0 verified (no button wired). Added `Button GPIO: %d` to selftest output.
- [ ] 5 presses within 5 seconds triggers selftest and LED blink codes display
  - BLOCKED: No physical button wired to P0.07 yet
- [ ] Button wired between P0.07 (WB_IO2) and VDD on the RAK5005-O baseboard
  - BLOCKED: Hardware not yet assembled

## Testing Requirements
- [x] Manual: press button, observe shell output — GPIO reads 0 (no button)
- [ ] Manual: 5-press trigger, observe LED blink pattern — BLOCKED (no hardware)

## Results (2026-02-17)
- Added `Button GPIO: %d` readout to `selftest_run_shell()` in `selftest.c`
- Verified GPIO reads 0 on device when no button wired (correct — pull-down)
- Remaining checks require physical button to be soldered to P0.07
- Full results: `tests/e2e/RESULTS-task072-button-gpio.md` on branch

## Deliverables
- [x] Code change: selftest.c button GPIO readout (committed on branch)
- [x] Partial verification log
- [ ] Full verification after button hardware installed
