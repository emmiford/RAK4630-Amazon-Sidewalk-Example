# TASK-109: PRD §2.0.3a — Definitive pin name cross-reference table

**Status**: committed (2026-02-22, Pam)
**Priority**: P2
**Owner**: Pam
**Branch**: `task/109-prd-pin-mapping`
**Size**: XS (0.5 point)

## Description
Add a once-and-for-all pin name cross-reference table to the PRD mapping between nRF52840 GPIO names, SAADC channel names, WisBlock connector labels, RAK19007 J11 silkscreen names, and RAK19001 J10/J15 labels. This eliminates the naming confusion that caused the 18-day firmware bug (EXP-009b).

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] Table inserted into PRD after §2.0.3 and before §2.0.3.1
- [x] Covers analog pins (AIN0, AIN1), digital IO pins (IO1-IO7), power pins
- [x] Includes critical naming hazard callout (WisBlock AIN1 ≠ nRF52840 AIN1)
- [x] Includes RAK19007 J11 header pinout diagram
- [x] Includes connector geometry note from EXP-010
- [ ] TODO: Verify RAK19001 J10/J15 silkscreen labels against physical board

## Deliverables
- PRD §2.0.3a section with all pin mapping tables — committed on `task/109-prd-pin-mapping`
