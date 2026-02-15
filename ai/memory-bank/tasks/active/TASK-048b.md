# TASK-048b: Charge Now single-press — implement FLAG_CHARGE_NOW button handler

**Status**: not started
**Priority**: P2
**Owner**: Bobby
**Branch**: `task/048-charge-now-button`
**Size**: S (2 points)

## Description
The `FLAG_CHARGE_NOW` bit (bit 3 of uplink byte 7) is reserved in app_tx.c but not yet implemented. TASK-040 added the button GPIO polling infrastructure (`EVSE_PIN_BUTTON`, pin 3). This task implements single-press behavior: a single press of the Charge Now button toggles charging (if paused → allow). Sets `FLAG_CHARGE_NOW` in the next uplink so the cloud knows it was a local override. Must coexist with 5-press self-test trigger.

Note: This was originally numbered TASK-048 in the monolithic task list (duplicate ID). Renamed to TASK-048b to resolve the conflict.

## Dependencies
**Blocked by**: TASK-040 (button GPIO infrastructure — DONE)
**Blocks**: none

## Acceptance Criteria
- [ ] Single press on Charge Now button toggles charging allowed state
- [ ] FLAG_CHARGE_NOW (bit 3) set in uplink byte 7 on next send after button press
- [ ] Flag auto-clears after one uplink
- [ ] Coexists with 5-press self-test trigger (no false triggers in either direction)
- [ ] Debounce: rapid presses within 200ms treated as one press

## Testing Requirements
- [ ] C unit tests: single-press toggles charge state
- [ ] C unit tests: FLAG_CHARGE_NOW set and auto-cleared
- [ ] C unit tests: 5-press still triggers self-test (no regression)

## Deliverables
- Modified `selftest_trigger.c` or new `charge_now_button.c`
- Modified `app_tx.c`
- Unit tests
