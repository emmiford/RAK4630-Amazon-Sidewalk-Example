# TASK-026: Add app discovery and boot path tests

**Status**: in progress (2026-02-19, Eero)
**Priority**: P2
**Owner**: Eero
**Branch**: `task/026-boot-path-tests`
**Size**: M (3 points)

## Description
`discover_app_image()` in `app.c` validates magic and version at the app callback address. This boot path has zero test coverage. Silent failures here mean the device boots without app functionality with no indication to the operator (except missing telemetry).

Also covers: OTA message routing (cmd 0x20 → OTA engine, else → app), NULL app_cb safety, and timer interval bounds validation.

## Dependencies
**Blocked by**: TASK-024 (version mismatch behavior must be defined first — DONE)
**Blocks**: none

## Acceptance Criteria
- [x] Test: valid magic + version → app callbacks invoked
- [x] Test: wrong magic → app not loaded, platform boots standalone
- [x] Test: wrong version → app not loaded (after TASK-024 hardens this)
- [x] Test: OTA message (cmd 0x20) routed to OTA engine, not app
- [x] Test: non-OTA message routed to app_cb->on_msg_received
- [x] Test: app_cb NULL → messages handled safely (no crash)
- [x] Test: timer interval bounds (< 100ms rejected, > 300000ms rejected)

## Testing Requirements
- [x] All tests use host-side C framework (Makefile, same pattern as test_app.c)
- [x] Mock app callback table with controllable magic/version

## Deliverables
- `tests/test_boot_path.c` — 10 tests covering all acceptance criteria + edge cases
- `tests/mock_boot.c` — stub implementations for Zephyr/Sidewalk dependencies
- `tests/mock_include/` — mock headers enabling host-side compilation of app.c
- `src/app.c` — HOST_TEST guards for discover_app_image, new `app_route_message()`
- `include/app.h` — declares `app_route_message()`
- `src/sidewalk_dispatch.c` — refactored to use `app_route_message()`

## Implementation Notes
- Extended Grenning dual-target pattern to platform code (app.c) via mock headers
- Extracted message routing from `sidewalk_dispatch.c` into testable `app_route_message()` in `app.c`
- Added minimal `#ifdef HOST_TEST` guards in `app.c` for test-controlled callback table address
- 10 tests total: 5 discovery, 4 routing (incl NULL safety + NULL callback), 1 timer bounds
