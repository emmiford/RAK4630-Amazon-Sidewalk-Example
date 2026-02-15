# TASK-035: Uplink payload v0x07 — add 4-byte timestamp and control flags

**Status**: merged done (2026-02-14, Eliel)
**Branch**: `task/035-uplink-v07`
**Merged**: c4abdda

## Summary
Bumped uplink payload from v0x06 (8 bytes) to v0x07 (12 bytes). Added 4-byte SideCharge epoch timestamp (bytes 8-11) and repurposed thermostat byte bits 2-7 for CHARGE_ALLOWED, CHARGE_NOW, SENSOR_FAULT, CLAMP_MISMATCH, INTERLOCK_FAULT, SELFTEST_FAIL flags. Decode Lambda handles both v0x06 and v0x07. 12 v0x07 Python tests + existing C tests updated.

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c`: v0x07 encoding (12 bytes)
- `aws/decode_evse_lambda.py`: v0x07 + v0x06 backward-compatible decoding
- `aws/tests/test_decode_evse.py`: 12 new v0x07 tests
