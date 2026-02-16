# TASK-056: Break up app.c into focused platform modules

**Status**: merged done (2026-02-16, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: task/056-breakup-app-c
**Size**: L (5 points)

## Summary
Split 653-line app.c god object into three focused platform modules. Deleted duplicate sid_shell.c. All host tests pass, platform build succeeds.

## Deliverables
- `src/app.c` (204 lines) — boot sequence, app image discovery, timer
- `src/sidewalk_dispatch.c` (195 lines) — Sidewalk event callbacks, BLE GATT auth
- `src/platform_shell.c` (330 lines) — all platform shell commands
- `include/sidewalk_dispatch.h` — dispatch API header
- `include/app.h` — added accessor functions (app_image_valid, app_get_callbacks, app_get_reject_reason)
- Deleted: `src/sid_shell.c` (consolidated into platform_shell.c)

## Follow-up
On-device shell verification tracked in TASK-058.
