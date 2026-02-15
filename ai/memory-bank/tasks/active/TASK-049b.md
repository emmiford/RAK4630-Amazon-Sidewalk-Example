# TASK-049b: Platform button callback — add GPIO interrupt-driven on_button_press to platform API

**Status**: not started
**Priority**: P3
**Owner**: Eliel
**Branch**: `task/049-platform-button-callback`
**Size**: M (3 points)

## Description
TASK-040's 5-press detection uses 500ms GPIO polling, which required widening the detection window from 3s to 5s. Adding an `on_button_press` callback to `app_callbacks` (driven by GPIO interrupt in platform) would give sub-millisecond press detection, enabling the original 3-second window. Additive change to `app_callbacks` (new field at end), requires bumping `APP_CALLBACK_VERSION` per ADR-001.

Note: This was originally numbered TASK-049 in the monolithic task list (duplicate ID). Renamed to TASK-049b to resolve the conflict.

## Dependencies
**Blocked by**: none
**Blocks**: tighter 3s detection window for TASK-040/TASK-048

## Acceptance Criteria
- [ ] `on_button_press(uint32_t timestamp_ms)` callback added to `app_callbacks` struct
- [ ] `APP_CALLBACK_VERSION` bumped (per ADR-001)
- [ ] Platform registers GPIO interrupt on button pin, calls callback on rising edge
- [ ] `selftest_trigger.c` updated to use callback instead of polling (detection window back to 3s)
- [ ] Backward compatible: platform checks callback version before calling

## Testing Requirements
- [ ] C unit tests: callback invocation triggers press detection
- [ ] C unit tests: 5 presses within 3s now triggers self-test

## Deliverables
- Modified `platform_api.h` (version bump + new field)
- Modified platform button init code
- Modified `selftest_trigger.c`
- Unit tests
