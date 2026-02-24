# REC-009: External Button/Potentiometer Assembly Validation

**Status**: Proposed
**Verdict**: —
**Type**: Hardware diagnostic
**Priority**: Medium
**Owner**: Oliver
**Related**: EXP-010

---

## Problem Statement

During EXP-010 input testing, all button-based input tests initially appeared to fail. The button/potentiometer assembly may be miswired, damaged, or require a different pull-up configuration.

## Hypothesis

The external button/potentiometer assembly has a wiring fault independent of the RAK19001 connector issue.

**Success Metrics**:
- Primary: button produces clean low/high transitions; potentiometer produces linear 0-3.3V sweep

## Method

**Step 1**: Multimeter continuity test on the button assembly (press/release)
**Step 2**: Measure potentiometer resistance range (wiper to each end)
**Step 3**: Connect button to a known-good IO pin (e.g., IO1/P0.17, confirmed working in EXP-010) and test input
**Step 4**: Connect potentiometer output to AIN1 after connector reseat, sweep voltage range
**Duration**: 15 minutes
