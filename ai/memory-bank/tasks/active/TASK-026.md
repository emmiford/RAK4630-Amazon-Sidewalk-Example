# TASK-026: Add app discovery and boot path tests

**Status**: not started
**Priority**: P2
**Owner**: Eero
**Branch**: `feature/boot-path-tests`
**Size**: M (3 points)

## Description
`discover_app_image()` in `app.c` validates magic and version at the app callback address. This boot path has zero test coverage. Silent failures here mean the device boots without app functionality with no indication to the operator (except missing telemetry).

Also covers: OTA message routing (cmd 0x20 → OTA engine, else → app), NULL app_cb safety, and timer interval bounds validation.

## Dependencies
**Blocked by**: TASK-024 (version mismatch behavior must be defined first — DONE)
**Blocks**: none

## Acceptance Criteria
- [ ] Test: valid magic + version → app callbacks invoked
- [ ] Test: wrong magic → app not loaded, platform boots standalone
- [ ] Test: wrong version → app not loaded (after TASK-024 hardens this)
- [ ] Test: OTA message (cmd 0x20) routed to OTA engine, not app
- [ ] Test: non-OTA message routed to app_cb->on_msg_received
- [ ] Test: app_cb NULL → messages handled safely (no crash)
- [ ] Test: timer interval bounds (< 100ms rejected, > 300000ms rejected)

## Testing Requirements
- [ ] All tests use Unity/CMake framework
- [ ] Mock app callback table with controllable magic/version

## Deliverables
- `tests/app/test_boot_path.c`
