# TASK-039: Commissioning self-test — boot self-test, continuous monitoring, `sid selftest`

**Status**: MERGED DONE (2026-02-14, Eero)
**Branch**: `feature/selftest` (commit `877d357`, merge `75cb85f`)

## Summary
Three self-test layers: (1) Boot self-test — ADC channels, GPIO pins, relay readback, Sidewalk init. (2) Continuous monitoring — current vs J1772 cross-check, charge enable effectiveness, pilot voltage range, thermostat chatter. (3) `sid selftest` shell command. Fault flags in uplink byte 7 bits 4-7 (SENSOR_FAULT, CLAMP_MISMATCH, INTERLOCK_FAULT, SELFTEST_FAIL). 23 C + 4 Python tests. On-device verification tracked in TASK-048.

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/selftest.c`
- `app/rak4631_evse_monitor/include/selftest.h`
- `tests/app/test_selftest.c` (23 tests)
- `aws/tests/test_decode_selftest.py` (4 tests)
