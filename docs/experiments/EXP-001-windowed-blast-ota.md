# EXP-001: Windowed Blast OTA Transfer Mode

**Status**: Concluded
**Verdict**: REVERTED
**Type**: Feature A/B
**Date**: pre-2026-02
**Owner**: Oliver
**Related**: EXP-004 (superseded by delta OTA)

---

## Problem Statement

Per-chunk ACK mode for OTA over LoRa is slow — each 15B chunk requires a round-trip ACK before the next chunk can be sent, making full-image OTA take hours.

## Hypothesis

Sending chunks on a timer within fixed-size windows (without waiting for per-chunk ACKs) will significantly reduce OTA transfer time by eliminating ACK-wait from the critical path.

**Success Metrics**:
- Primary: reduction in total OTA transfer time for full-image updates

## Method

**Variants**:
- Control: Legacy per-chunk ACK mode (send chunk → wait ACK → send next)
- Variant: Windowed blast mode (send N chunks on timer → device reports gaps → gap-fill)

**Implementation**: Commit `e3f97e0` — device tracks received chunks via bitfield, reports gaps at window boundaries. Backward-compatible via `window_size=0` in OTA_START.

## Results

**Decision**: REVERTED (commit `78924b6`)
**Rationale**: Delta OTA mode (EXP-004) made blast mode obsolete before it could be fully validated in production. Delta sends 2-3 chunks for typical changes vs 277+ for full image. The complexity of windowed blast (gap tracking, window ACKs, timer scheduling) was no longer justified.
**Outcome**: Code removed. Delta mode + legacy per-chunk ACK retained.

## Key Insights

- The right abstraction level matters more than optimizing the wrong approach. Sending fewer chunks (delta) beats sending the same chunks faster (blast).
- Blast mode added significant protocol complexity for a problem that was better solved at a higher level.
- Backward-compatible protocol design (`window_size=0` fallback) was good practice — made revert clean.
