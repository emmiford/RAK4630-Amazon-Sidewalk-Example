# EXP-004: Delta OTA Mode

**Status**: Concluded
**Verdict**: GO
**Type**: Feature A/B
**Date**: pre-2026-02
**Owner**: Oliver
**Related**: EXP-001 (supersedes), EXP-002, EXP-003

---

## Problem Statement

Full-image OTA over LoRa sends the entire firmware binary (~3.9KB, 260 chunks at 15B each) even when only a few bytes changed. At LoRa data rates with ACK round-trips, this takes hours.

## Hypothesis

Comparing new firmware against a stored baseline and sending only changed chunks will reduce OTA transfer time from hours to seconds for typical app changes.

**Success Metrics**:
- Primary: orders-of-magnitude reduction in chunk count and transfer time for incremental updates

## Method

**Variants**:
- Control: Full-image OTA (260+ chunks, hours)
- Variant: Delta OTA (only changed chunks, seconds)

**Implementation** (commit `65e7389`):
- Cloud: Lambda computes chunk diff against S3 baseline, sends only changed chunks with absolute indexing, auto-saves successful firmware as new baseline
- Device: Delta receive mode with bitfield tracking, merged CRC validation (staging + primary), page-by-page apply with baseline overlay

## Results

**Decision**: GO — merged to main (via `eed3aeb`)
**Primary Metric Impact**: 2-3 chunks (~seconds) vs 277+ chunks (~hours) for typical app changes. ~100x improvement.
**Secondary Impact**: Made windowed blast mode (EXP-001) obsolete — reverted.
**Verification**: Deploy CLI (`ota_deploy.py preview`) shows delta before sending.

## Key Insights

- This was the single most impactful experiment in the project. Changed OTA from "overnight operation" to "deploy while you watch."
- Baseline management is critical — S3 stores the last successful firmware as reference. If baseline drifts, delta fails gracefully (falls back to full).
- The `ota_deploy.py` tooling around delta (preview, baseline, deploy) made the feature practical, not just possible.
