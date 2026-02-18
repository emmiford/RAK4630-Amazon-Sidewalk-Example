# TASK-067: LED blink priority state machine

**Status**: merged done (2026-02-17, Eliel)
**Priority**: P1
**Size**: M (3 points)

## Summary
Table-driven LED blink engine with 8 priority levels (error 5Hz, OTA double-blink,
commissioning 1Hz, disconnected triple-blink, charge-now 0.5Hz, AC priority heartbeat,
charging solid, idle blip every 10s). Timer changed from 500ms to 100ms with 5-count
decimation for sensor logic. 21 new unit tests (96 total).

## Deliverables
- `include/led_engine.h` — public API
- `src/app_evse/led_engine.c` — blink engine implementation
- `app_entry.c` — timer 500ms → 100ms, decimation, LED engine integration
- `mock_platform.h/.c` — LED call history array for pattern verification
- `test_app.c` — 21 new tests
- `Makefile` + `CMakeLists.txt` — build integration
