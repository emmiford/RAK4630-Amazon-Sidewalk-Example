# TASK-058: On-device shell verification (post app.c refactor)

**Status**: not started
**Priority**: P1
**Owner**: Eero
**Branch**: —
**Size**: S (1 point)

## Description
After TASK-056 split app.c into three files (app.c, platform_shell.c, sidewalk_dispatch.c), verify all shell commands still work on-device via serial console. This is a smoke test — no code changes expected, just flash and verify.

## Dependencies
**Blocked by**: TASK-056 (merged done)
**Blocks**: none

## Acceptance Criteria
- [ ] Flash platform + app to device
- [ ] `sid status` responds with Sidewalk init state and link info
- [ ] `sid mfg` responds with MFG store version and device ID
- [ ] `sid ota status` responds with OTA phase
- [ ] `app evse status` responds with J1772 state, voltage, current
- [ ] `sid selftest` runs commissioning self-test
- [ ] `sid lora` / `sid ble` switch link type without crash

## Testing Requirements
- [ ] Manual serial console verification (`/dev/tty.usbmodem101`)
