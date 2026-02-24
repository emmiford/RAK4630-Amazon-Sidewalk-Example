# REC-005: OTA Reliability Under Real-World LoRa Conditions

**Status**: Proposed (test plan written)
**Verdict**: —
**Type**: Field measurement
**Priority**: High
**Owner**: Oliver
**Related**: EXP-001, EXP-002, EXP-003, EXP-004

---

## Problem Statement

OTA has only been tested in controlled lab conditions. Real-world LoRa conditions (distance, interference, weather) will have higher packet loss rates that could stress the retry/recovery mechanisms. If OTA fails in the field with no physical access, the device is bricked. This testing is safety-critical.

## Hypothesis

Characterizing OTA success rate, transfer time, and retry count across varying RF conditions will validate the current reliability parameters (5 retries, 30s stale threshold, 1-min timer).

**Success Metrics**:
- Primary: OTA success rate per condition (lab, indoor, outdoor near, outdoor far)
- Guardrail: recovery from power-cycle during apply (safety-critical)

## Method

### Prerequisites
- Two firmware versions ready for alternating deploys (version N and N+1)
- Device flashed and Sidewalk-connected
- `ota_deploy.py` working (baseline captured)
- Serial access for device-side verification

### Test Conditions

| Condition | Distance | Environment | Expected Signal |
|-----------|----------|-------------|-----------------|
| Lab | ~1m | Same room as gateway | Excellent (RSSI > -80) |
| Indoor | ~10m | Different floor/room | Good (RSSI -80 to -110) |
| Near outdoor | ~50m | Outside, line of sight | Moderate (RSSI -110 to -130) |
| Far outdoor | ~200m+ | Outside, obstructions | Poor (RSSI < -130) |

### Per-Cycle Procedure (3 cycles per condition)

1. Record baseline: `python3 aws/ota_deploy.py baseline`
2. Deploy: `python3 aws/ota_deploy.py deploy --build --version <N>`
3. Monitor: `python3 aws/ota_deploy.py status --watch`
4. Record: start time, end time, transfer time, mode (delta/full), chunks sent/retransmitted, RSSI, result
5. Verify: device reboots, `sid status` shows new version, `app evse status` works

### Power-Cycle Test (once per condition set)

1. Start OTA deploy
2. Wait until status shows APPLYING
3. Power-cycle the device (unplug USB)
4. Power back on
5. Verify: device recovers and completes OTA (or boots with previous version)
6. Record: recovery behavior, time to recover

### Success Criteria

| Metric | Threshold | Rationale |
|--------|-----------|-----------|
| Lab success rate | 100% (3/3) | Baseline — must work perfectly |
| Indoor success rate | 100% (3/3) | Typical deployment location |
| Near outdoor success rate | ≥67% (2/3) | May require retries |
| Far outdoor success rate | ≥33% (1/3) | Edge case, good to quantify |
| Power-cycle recovery | 100% | Safety-critical |
| Max transfer time (delta, lab) | <60s | Delta should be fast |
| Max transfer time (full, lab) | <90 min | ~276 chunks at LoRa rate |

### Go/No-Go Decision

- **GO for production**: Lab + Indoor 100%, power-cycle recovery works, far outdoor degrades gracefully
- **NO-GO**: Any lab failure, any power-cycle recovery failure, or indoor <67%
- **CONDITIONAL GO**: Indoor <100% — investigate retry parameters

### Parameters to Tune (if needed)

| Parameter | Current Value | Where |
|-----------|--------------|-------|
| EventBridge retry interval | 60s | `aws/terraform/main.tf` |
| Max retries before abort | 10 | `aws/ota_sender_lambda.py` |
| Apply delay | 15s | `app/rak4631_evse_monitor/src/ota_update.c` |
| Chunk size | 15 bytes | `aws/ota_sender_lambda.py` (LoRa MTU - overhead) |

## Key Notes

- LoRa Class A: device only receives downlinks in short RX windows after uplinks. OTA chunks are paced by EventBridge timer, not raw LoRa throughput.
- Delta mode: 2-3 chunks. Full mode: ~276 chunks.
- 60s heartbeat uplinks open receive windows for OTA chunks.

## References

- Files: `aws/ota_deploy.py`, `aws/ota_sender_lambda.py`, `app/rak4631_evse_monitor/src/ota_update.c`
- Tasks: TASK-005, TASK-007, TASK-008
