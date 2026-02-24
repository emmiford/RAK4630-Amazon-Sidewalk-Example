# REC-002: ADC Polling Interval Optimization (Currently 500ms)

**Status**: Proposed
**Verdict**: —
**Type**: Parameter tuning
**Priority**: Medium
**Owner**: Oliver
**Related**: EXP-005 (on-change sensing)

---

## Problem Statement

The 500ms ADC polling interval for J1772 pilot and current clamp was chosen as a reasonable default but hasn't been validated against actual J1772 state transition speeds or battery impact.

## Hypothesis

J1772 pilot state transitions happen on the order of seconds (relay switching). A longer polling interval (1s, 2s) may catch all transitions while reducing CPU wake-ups and battery drain.

**Success Metrics**:
- Primary: missed state transitions (compare against reference J1772 pilot logger)
- Secondary: CPU active time / battery impact
- Guardrail: never miss a State A→C transition (vehicle plugged in and ready)

## Method

**Variants**: 250ms, 500ms (current), 1000ms, 2000ms
**Duration**: 1 week per variant with daily plug/unplug cycles
**Prerequisites**: Reference J1772 pilot logger for ground truth, current measurement setup
