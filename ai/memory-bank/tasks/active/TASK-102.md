# TASK-102: Reseat RAK4631 in RAK19001 and validate all even-row pins

**Status**: not started
**Priority**: P1
**Owner**: —
**Branch**: —
**Size**: S (1 point)

## Description
EXP-010 identified that one entire row (even-numbered pins) of the RAK4631-to-RAK19001 40-pin board-to-board connector has no electrical contact due to incomplete module seating. This blocks all further hardware development on the RAK19001 baseboard.

Physically reseat the RAK4631 module with firm, even pressure until both rows of the Hirose DF40C connector click into place. Then systematically validate all previously-failing even-row pins: IO2 (P1.02), IO4 (P0.04), IO6 (P0.10), and AIN1 (P0.31/AIN7).

Reference: Oliver's experiment log EXP-010 root cause analysis and recommendation REC-007.

## Dependencies
**Blocked by**: none
**Blocks**: all further RAK19001 hardware bring-up, TASK-103 (button/pot validation)

## Acceptance Criteria
- [ ] RAK4631 module physically reseated in RAK19001 baseboard — both connector rows engaged
- [ ] Multimeter continuity check passes on all even-row connector pins (22, 30, 32, 38) between module pad and baseboard test point
- [ ] IO2 (P1.02, pin 30) GPIO output test passes (LED toggle) — was FAIL in EXP-010
- [ ] IO2 (P1.02, pin 30) GPIO input test passes (wire short to GND) — was FAIL in EXP-010
- [ ] AIN1 (P0.31, pin 22) ADC reads correct voltage from applied source — was 0mV in EXP-010
- [ ] IO4 (P0.04, pin 32) and IO6 (P0.10, pin 38) pass GPIO output/input tests
- [ ] Firmware reverted to original pin assignments: charge_block=IO1/P0.17, cool_call=IO2/P1.02, pilot ADC=AIN7/P0.31

## Testing Requirements
- [ ] All tests performed with original firmware (no workaround pin swaps)
- [ ] Results logged in experiment log as REC-007 follow-up

## Deliverables
- Reseated hardware with all even-row pins validated
- Updated experiment log entry confirming fix (or escalation if reseat does not resolve)
