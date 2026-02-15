# TASK-033: TIME_SYNC downlink — 0x30 command with SideCharge epoch and ACK watermark

**Status**: MERGED DONE (2026-02-14, Eliel)
**Branch**: `task/033-time-sync-downlink` (commit `399c8e3`)

## Summary
Device: `time_sync.c/h` parses 9-byte 0x30 command (epoch + ACK watermark), tracks wall-clock time via uptime offset. `app_rx.c` dispatches 0x30. `app_entry.c` adds time_sync_init/set_api + `sid time` shell command. Cloud: `decode_evse_lambda.py` auto-sends TIME_SYNC on EVSE uplinks with DynamoDB sentinel (daily re-sync). 11 C + 15 Python tests, all passing. Pending: on-device verification (TASK-047).

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/time_sync.c`
- `app/rak4631_evse_monitor/include/time_sync.h`
- `aws/decode_evse_lambda.py` (auto-sync trigger)
- `tests/app/test_time_sync.c` (11 tests)
- `aws/tests/test_time_sync.py` (15 tests)
