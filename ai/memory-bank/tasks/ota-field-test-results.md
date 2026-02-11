# OTA Field Reliability Test Plan

**Status**: PLAN WRITTEN — awaiting execution
**Reference**: Experiment Log REC-005

## Objective

Validate OTA reliability across real-world LoRa RF conditions. OTA has only been tested in controlled lab conditions. If OTA fails in the field with no physical access, the device is bricked. This testing is safety-critical.

## Prerequisites

- Two firmware versions ready for alternating deploys (e.g., version N and N+1 with minor differences)
- Device flashed and Sidewalk-connected
- `ota_deploy.py` working (baseline captured)
- Serial access for device-side verification

## Test Conditions

| Condition | Distance | Environment | Expected Signal |
|-----------|----------|-------------|-----------------|
| Lab | ~1m | Same room as gateway | Excellent (RSSI > -80) |
| Indoor | ~10m | Different floor/room | Good (RSSI -80 to -110) |
| Near outdoor | ~50m | Outside, line of sight | Moderate (RSSI -110 to -130) |
| Far outdoor | ~200m+ | Outside, obstructions | Poor (RSSI < -130) |

## Test Procedure

For each condition, perform **3 OTA cycles** (alternating between version N and N+1):

### Per-Cycle Steps

1. **Record baseline**:
   ```
   python3 aws/ota_deploy.py baseline
   ```

2. **Deploy**:
   ```
   python3 aws/ota_deploy.py deploy --build --version <N or N+1>
   ```

3. **Monitor**:
   ```
   python3 aws/ota_deploy.py status --watch
   ```

4. **Record**:
   - Start time
   - End time (COMPLETE received)
   - Total transfer time
   - Mode: delta or full
   - Chunks sent / retransmitted
   - Final status: SUCCESS or FAIL
   - Device RSSI (from DynamoDB or `sid status`)

5. **Verify**:
   - Device reboots automatically
   - `sid status` shows new version (or check boot log)
   - `app evse status` works (app functional)

### Power-Cycle Test (once per condition set)

1. Start an OTA deploy
2. Wait until status shows APPLYING
3. Power-cycle the device (unplug USB)
4. Power back on
5. Verify: device recovers and completes OTA (or boots with previous version)
6. Record: recovery behavior, time to recover

## Results Template

### Condition: [Lab / Indoor / Near outdoor / Far outdoor]

| Cycle | Version | Mode | Duration | Chunks Sent | Retransmits | RSSI | Result |
|-------|---------|------|----------|-------------|-------------|------|--------|
| 1 | N→N+1 | delta | | | | | |
| 2 | N+1→N | delta | | | | | |
| 3 | N→N+1 | delta | | | | | |

**Power-cycle test**: [PASS/FAIL] — [notes]

## Success Criteria

| Metric | Threshold | Rationale |
|--------|-----------|-----------|
| Lab success rate | 100% (3/3) | Baseline — must work perfectly |
| Indoor success rate | 100% (3/3) | Typical deployment location |
| Near outdoor success rate | ≥67% (2/3) | May require retries |
| Far outdoor success rate | ≥33% (1/3) | Edge case, good to quantify |
| Power-cycle recovery | 100% | Safety-critical |
| Max transfer time (delta, lab) | <60s | Delta should be fast |
| Max transfer time (full, lab) | <90 min | ~276 chunks at LoRa rate |

## Go/No-Go Decision

- **GO for production**: Lab + Indoor 100%, power-cycle recovery works, far outdoor degrades gracefully
- **NO-GO**: Any lab failure, any power-cycle recovery failure, or indoor success rate <67%
- **CONDITIONAL GO**: Indoor <100% — investigate retry parameters, tune `OTA_APPLY_DELAY_SEC` or chunk retry logic

## Parameters to Tune (if needed)

| Parameter | Current Value | Where |
|-----------|--------------|-------|
| EventBridge retry interval | 60s | aws/terraform/main.tf |
| Max retries before abort | 10 | aws/ota_sender_lambda.py |
| Apply delay | 15s | app/rak4631_evse_monitor/src/ota_update.c |
| Chunk size | 15 bytes | aws/ota_sender_lambda.py (LoRa MTU - overhead) |

## Notes

- LoRa Class A: device only receives downlinks in short RX windows after uplinks. OTA chunks are paced by the EventBridge retry timer, not by raw LoRa throughput.
- Delta mode typically sends 2-3 chunks. Full mode sends ~276 chunks.
- The device sends periodic uplinks (60s heartbeat), which opens receive windows for OTA chunks.
