# TASK-052: Rename rak_sidewalk → evse_payload

**Status**: merged done (2026-02-15, Eliel)
**Priority**: P1
**Owner**: Eliel
**Size**: S (1 point)

## Summary
Renamed `rak_sidewalk.c` → `evse_payload.c`, merged `rak_sidewalk.h` into `evse_payload.h`, and renamed all functions (`evse_payload_set_api`, `evse_payload_get`, `evse_payload_init`). Updated 13 files across source, tests, build files, and docs. All 164 C tests and 151 Python tests pass.
