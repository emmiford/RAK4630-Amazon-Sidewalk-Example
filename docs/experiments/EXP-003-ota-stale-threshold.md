# EXP-003: OTA Retry Stale Threshold Reduction (300s → 30s)

**Status**: Concluded
**Verdict**: GO
**Type**: Parameter tuning
**Date**: pre-2026-02
**Owner**: Oliver
**Related**: EXP-002, EXP-004

---

## Problem Statement

Lost LoRa ACKs caused 5-minute dead waits before the EventBridge retry timer would resend. This was the dominant source of OTA transfer latency.

## Hypothesis

Reducing the stale session threshold from 300s to 30s will allow faster recovery from lost ACKs, significantly reducing total OTA transfer time.

**Success Metrics**:
- Primary: reduction in worst-case stall duration per lost ACK

## Method

**Variants**:
- Control: 300s stale threshold (5-min dead wait per lost ACK)
- Variant: 30s stale threshold (caught on next timer tick, ~60s worst case)

## Results

**Decision**: GO — merged to main (commit `3db368b`)
**Primary Metric Impact**: Worst-case stall reduced from ~300s to ~60s per lost ACK. Commit message states "roughly halving total OTA transfer time."
**Risk**: More aggressive retries could cause duplicate chunks, but device-side deduplication (bitfield tracking) handles this safely.

## Key Insights

- LoRa packet loss is a first-class concern, not an edge case. The system needs to be tuned for lossy links.
- 300s was overly conservative — likely a default that was never calibrated against real-world LoRa conditions.
- The EventBridge 1-minute timer granularity sets the floor. 30s threshold + 60s timer = ~60s worst case, which is a good balance.
