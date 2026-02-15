# TASK-035: Uplink payload v0x07 — add 4-byte timestamp and control flags

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: `feature/uplink-v07`
**Size**: M (3 points)

## Description
Per PRD 3.2.1, the current 8-byte uplink payload (v0x06) has no device-side timestamp, no charge_control state, and no Charge Now flag. This task bumps to v0x07 (12 bytes): adds 4-byte SideCharge epoch timestamp (bytes 8-11) and repurposes thermostat byte bits 2-7 for CHARGE_ALLOWED, CHARGE_NOW, SENSOR_FAULT, CLAMP_MISMATCH, INTERLOCK_FAULT, and SELFTEST_FAIL flags. Decode Lambda must handle both v0x06 and v0x07 for backward compatibility.

## Dependencies
**Blocked by**: TASK-033 (TIME_SYNC needed for valid timestamps — DONE), TASK-034 (event buffer provides snapshots — DONE)
**Blocks**: TASK-047 (needs all 3 features merged for device verification)

## Acceptance Criteria
- [ ] Uplink payload version bumped to 0x07
- [ ] Bytes 8-11: SideCharge epoch timestamp (seconds since 2026-01-01), little-endian uint32
- [ ] Thermostat byte (byte 7) bits 2-7: CHARGE_ALLOWED, CHARGE_NOW, SENSOR_FAULT, CLAMP_MISMATCH, INTERLOCK_FAULT, SELFTEST_FAIL
- [ ] Total payload: 12 bytes (fits within 19-byte LoRa MTU)
- [ ] Decode Lambda handles both v0x06 (8 bytes) and v0x07 (12 bytes)
- [ ] DynamoDB events include device-side timestamp when available

## Testing Requirements
- [ ] C unit tests: v0x07 payload encoding (all fields, boundary values)
- [ ] C unit tests: timestamp computation from synced time
- [ ] Python tests: decode Lambda v0x07 parsing
- [ ] Python tests: backward compatibility with v0x06

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c`: v0x07 encoding
- `aws/decode_evse_lambda.py`: v0x07 + v0x06 decoding
- `tests/app/test_payload_v07.c`
- Updated `aws/tests/test_decode_evse.py`
