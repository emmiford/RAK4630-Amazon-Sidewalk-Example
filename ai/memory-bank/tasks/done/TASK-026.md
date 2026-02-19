# TASK-026: Add app discovery and boot path tests

**Status**: merged done (2026-02-19, Eero)
**Priority**: P2
**Owner**: Eero
**Branch**: task/026-boot-path-tests
**Size**: M (3 points)

## Description
Added host-side unit tests for the platform boot path: `discover_app_image()` magic/version
validation, OTA vs app message routing, NULL safety, and timer interval bounds. Extended the
Grenning dual-target pattern to platform code via mock Zephyr/Sidewalk headers.

## Acceptance Criteria
- [x] Test: valid magic + version → app callbacks invoked
- [x] Test: wrong magic → app not loaded, platform boots standalone
- [x] Test: wrong version → app not loaded
- [x] Test: OTA message (cmd 0x20) routed to OTA engine, not app
- [x] Test: non-OTA message routed to app_cb->on_msg_received
- [x] Test: app_cb NULL → messages handled safely (no crash)
- [x] Test: timer interval bounds (< 100ms rejected, > 300000ms rejected)

## Deliverables
- `tests/test_boot_path.c` — 10 tests (7 criteria + 3 edge cases)
- `tests/mock_boot.c` + `tests/mock_include/` — host compilation of app.c
- `src/app.c` — HOST_TEST guards, new `app_route_message()`
- `include/app.h` — declares `app_route_message()`
- `src/sidewalk_dispatch.c` — refactored to use `app_route_message()`
