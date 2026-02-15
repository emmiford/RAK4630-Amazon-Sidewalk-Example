# TASK-034: Device-side event buffer — ring buffer with ACK watermark trimming

**Status**: MERGED DONE (2026-02-14, Eliel)
**Branch**: `task/034-event-buffer` (commit `88cb692`, merge `d37dee5`)

## Summary
50-entry ring buffer (600B RAM) of timestamped EVSE state snapshots. Captures J1772 state, voltage, current, thermostat flags, and charge control state on every 500ms poll. TIME_SYNC ACK watermark trims acknowledged entries. Integration: `app_entry.c` (init, snapshot on timer, `evse buffer` shell cmd), `app_rx.c` (trim after TIME_SYNC). 19 C tests, all passing. Pending: on-device verification (TASK-047).

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/event_buffer.c`
- `app/rak4631_evse_monitor/include/event_buffer.h`
- `tests/app/test_event_buffer.c` (19 tests)
