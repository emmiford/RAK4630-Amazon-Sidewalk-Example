# EXP-005: On-Change Sensing vs Fixed-Interval Polling

**Status**: Concluded
**Verdict**: GO
**Type**: Architecture change
**Date**: pre-2026-02
**Owner**: Oliver
**Related**: REC-001 (heartbeat tuning), REC-002 (ADC polling tuning), REC-003 (TX rate limiter)

---

## Problem Statement

Fixed-interval sensor polling wastes LoRa airtime transmitting unchanged readings. Sidewalk LoRa is bandwidth-constrained (19B MTU, shared spectrum).

## Hypothesis

Event-driven uplinks (transmit only on state change + periodic heartbeat) will dramatically reduce unnecessary transmissions while maintaining data freshness.

**Success Metrics**:
- Primary: reduction in uplink count without missing state transitions

## Method

**Variants**:
- Control: Fixed-interval polling (transmit every N seconds regardless of change)
- Variant: Change detection (500ms ADC poll with state comparison, TX only on change, 60s heartbeat)

**Implementation** (commit `deb4007`):
- Thermostat GPIOs: hardware edge interrupts
- J1772 pilot + current clamp: 500ms ADC polling with state comparison
- 5s TX rate limiter to avoid flooding on rapid state changes
- 60s heartbeat for liveness

## Results

**Decision**: GO — merged to main (via `170fbda`)
**Primary Metric Impact**: TX count drops to ~1/minute (heartbeat) during steady state, vs every-poll-cycle before. Bursts only on actual state transitions.
**Verification**: `sid test_change` shell command for on-device testing.

## Key Insights

- For a battery-constrained LoRa device, "don't transmit unless something changed" is the single most important power optimization.
- The 5s rate limiter was important — rapid J1772 pilot voltage transitions during plug-in could generate a burst of state changes.
- 60s heartbeat is a good liveness signal without being wasteful. Worth validating this interval (see REC-001).
