# REC-010: Connector Seating Force Measurement

**Status**: Proposed
**Verdict**: —
**Type**: Process evaluation
**Priority**: Medium
**Owner**: Oliver
**Related**: EXP-010, EXP-012

---

## Problem Statement

EXP-010 revealed that the Hirose DF40C 40-pin connector can appear physically attached while leaving one entire row disconnected (later identified as bad solder joints by EXP-012, but the connector seating risk remains real for production). Field installers may not apply sufficient seating force.

## Hypothesis

Measuring the force required for full engagement and defining a go/no-go tactile or visual indicator will prevent connector seating failures in production units.

**Success Metrics**:
- Primary: documented seating force threshold and pass/fail indicator

## Method

**Step 1**: With a spring scale, measure the insertion force required for full seating (both rows click)
**Step 2**: Identify tactile/audible feedback that distinguishes partial vs full seating
**Step 3**: Document a commissioning checklist step: "apply X grams of force until Y click/flush indicator"
**Step 4**: Consider an electrical continuity test as part of commissioning self-test (loopback between odd and even pin)

## References

- Tasks: TASK-095 (PCB design — only relevant once finalized)
