# REC-001: Heartbeat Interval Optimization (Currently 60s)

**Status**: Proposed
**Verdict**: —
**Type**: Parameter tuning
**Priority**: Medium
**Owner**: Oliver
**Related**: EXP-005 (on-change sensing)

---

## Problem Statement

The 60s heartbeat interval was chosen without empirical validation. It may be too frequent (wasting battery/airtime) or too infrequent (delayed anomaly detection on the cloud side).

## Hypothesis

There exists an optimal heartbeat interval that balances liveness detection latency against battery/airtime cost.

**Success Metrics**:
- Primary: cloud-side anomaly detection latency (time from device offline to alert)
- Secondary: battery impact (mAh consumed by heartbeat TX per day)
- Guardrail: no missed state transitions (on-change detection is independent)

## Method

**Variants**: 30s, 60s (current), 120s, 300s
**Duration**: 1 week per variant (stable state, no OTA or config changes)
**Prerequisites**: Battery current measurement setup, cloud-side alerting to detect offline devices
