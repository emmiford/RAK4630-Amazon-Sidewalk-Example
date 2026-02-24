# EXP-002: OTA Chunk Size Optimization (12B → 15B)

**Status**: Concluded
**Verdict**: GO
**Type**: Parameter tuning
**Date**: pre-2026-02
**Owner**: Oliver
**Related**: EXP-003, EXP-004

---

## Problem Statement

OTA chunks were using 12B data + 4B header = 16B, leaving 3 bytes of the 19B Sidewalk LoRa MTU unused.

## Hypothesis

Increasing chunk data from 12B to 15B (using full 19B MTU) will reduce the number of chunks required and proportionally reduce OTA transfer time.

**Success Metrics**:
- Primary: measurable reduction in chunk count for same firmware size

## Method

**Variants**:
- Control: 12B data chunks (325 chunks for ~3.9KB app)
- Variant: 15B data chunks (260 chunks for ~3.9KB app)

## Results

**Decision**: GO — merged to main (commit `b8e62cd`)
**Primary Metric Impact**: 20% reduction in chunk count (260 vs 325).
**Verification**: Device-side code already handled variable chunk sizes via OTA_START parameters — zero device code changes needed.

## Key Insights

- Always use your full MTU. The 3 unused bytes were pure waste.
- The device-side protocol was well-designed — chunk size parameterized in OTA_START, not hardcoded.
- 20% fewer chunks means 20% fewer ACK round-trips, compounding the benefit.
