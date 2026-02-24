# REC-003: TX Rate Limiter Tuning (Currently 100ms)

**Status**: Proposed
**Verdict**: —
**Type**: Parameter tuning
**Priority**: Low
**Owner**: Oliver
**Related**: EXP-005 (on-change sensing)

---

## Problem Statement

The TX rate limiter prevents sending more than one uplink per 100ms. This was set conservatively. On LoRa, actual TX + airtime may be longer, meaning the limiter may never trigger. Or it may be too aggressive during rapid state transitions.

## Hypothesis

The rate limiter threshold can be calibrated to the actual Sidewalk LoRa TX cadence to avoid both wasted attempts and delayed state reports.

**Success Metrics**:
- Primary: dropped/queued uplinks per day
- Secondary: state transition report latency

## Method

**Step 1**: Instrument actual TX timing (uplink queued → Sidewalk send callback → ACK) to establish baseline
**Step 2**: Set limiter to 1.5x actual TX cadence
**Prerequisites**: Shell logging of TX timestamps at each stage
