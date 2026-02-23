# TASK-102: Reflow RAK19001 even-row header pin solder joints

**Status**: not started (redefined 2026-02-22)
**Priority**: P2 (downgraded from P1 — RAK19007 is sufficient for current dev)
**Owner**: —
**Branch**: —
**Size**: XS (0.5 point)

## Description
~~EXP-010 identified a Hirose DF40C connector seating failure on the RAK19001.~~

**Updated (2026-02-22)**: EXP-012 Step 1.6 identified the actual root cause — **cold/bad solder joints on the hand-soldered even-row header pins** on the RAK19001 baseboard. The Hirose board-to-board connector and nRF52840 GPIOs are both fine. All even-row pins (IO2, IO4, IO6, AIN1) respond correctly when probed from the back side of the PCB.

This is a physical hardware rework task: reflow or resolder the even-row header pins. No firmware changes needed.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-103 (button/pot validation on RAK19001)

## Acceptance Criteria
- [ ] Even-row header pins reflowed/resoldered on RAK19001
- [ ] IO2 (pin 30), IO4 (pin 32), IO6 (pin 38) read HIGH via front-side headers when 3V3 applied
- [ ] AIN1 (pin 22) reads correct voltage via front-side header

## Deliverables
- RAK19001 with working even-row header pins
- Updated experiment log confirming fix
