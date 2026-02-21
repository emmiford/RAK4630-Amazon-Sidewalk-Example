# TASK-103: Validate external button and potentiometer hardware independently

**Status**: not started
**Priority**: P2
**Owner**: —
**Branch**: —
**Size**: S (1 point)

## Description
During EXP-010 input testing, all button-based input tests initially appeared to fail. The breakthrough of wire-shorting IO5 directly to GND (bypassing the button) proved the pins were functional and the external button/potentiometer assembly was broken or miswired. This issue was initially conflated with the connector seating failure and needs independent validation.

Test the button and potentiometer components in isolation using a multimeter, then connect them to known-good pins to confirm end-to-end functionality.

Reference: Oliver's experiment log EXP-010 key insights and recommendation REC-009.

## Dependencies
**Blocked by**: TASK-102 (connector reseat must be validated first so known-good pins are available)
**Blocks**: none

## Acceptance Criteria
- [ ] Multimeter continuity test on button assembly: clean open/closed transitions on press/release
- [ ] Multimeter resistance sweep on potentiometer: linear range measured (wiper to each end terminal)
- [ ] Button connected to known-good IO pin (e.g., IO1/P0.17, confirmed working in EXP-010) — firmware reads clean high/low transitions
- [ ] Potentiometer connected to AIN1 after connector reseat — firmware reads voltage sweep across full range (0-3.3V)
- [ ] If button or potentiometer is faulty, document the failure mode and source a replacement

## Testing Requirements
- [ ] All electrical tests performed with multimeter before connecting to MCU pins
- [ ] Firmware tests use pins confirmed working by TASK-102

## Deliverables
- Confirmed working (or documented faulty) button and potentiometer assembly
- EVSE simulator hardware fully validated for development use
