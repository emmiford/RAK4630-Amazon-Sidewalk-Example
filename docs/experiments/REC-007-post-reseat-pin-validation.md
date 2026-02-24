# REC-007: Post-Reseat RAK19001 Pin Validation

**Status**: Proposed
**Verdict**: —
**Type**: Hardware diagnostic
**Priority**: Critical
**Owner**: Oliver
**Related**: EXP-010, EXP-012

---

## Problem Statement

EXP-012 Step 1.6 identified cold/bad solder joints on the RAK19001's hand-soldered even-row header pins. After reflowing/resoldering these joints, all even-row pins need validation before the RAK19001 can be used for v1.1 features.

## Hypothesis

Reflowing the even-row header solder joints on the RAK19001 will restore electrical contact on all even-row pins (IO2, IO4, IO6, AIN1).

**Success Metrics**:
- Primary: all previously-failing even pins respond correctly
- Guardrail: no regressions on odd-row pins (IO1, IO3, IO5)

## Method

**Procedure**:
1. Reflow/resolder even-row header pins with proper technique
2. Before firmware: multimeter continuity check on all even pins (22, 30, 32, 38) between module pad and baseboard test point
3. Flash original firmware (charge_block=IO1, cool_call=IO2, ADC=AIN7)
4. Test IO2 GPIO output (LED toggle) — was FAIL in EXP-010
5. Test IO2 GPIO input (wire short to GND) — was FAIL in EXP-010
6. Test AIN1/P0.31 ADC (apply known voltage from potentiometer)
7. Optionally test IO4 and IO6 for completeness

**Duration**: 30 minutes
