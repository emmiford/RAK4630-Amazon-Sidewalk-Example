# TASK-056: Break up app.c into focused platform modules

**Status**: committed (2026-02-16, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: task/056-breakup-app-c
**Size**: L (5 points)

## Description
`app.c` is a 652-line god object with 6+ responsibilities: boot sequence, timer management, Sidewalk event handling, OTA message routing, shell command dispatch, and BLE GATT authorization.

Split into three focused files:

### `app.c` (~200 lines) — Boot only
- `app_start()`: discover app image, validate version, init OTA, check recovery, start Sidewalk, register timer
- `discover_app_image()`: magic/version validation at 0x90000
- Timer creation and start
- The "main" of the platform layer — readable top-to-bottom as a boot script

### `platform_shell.c` (~200 lines) — All platform shell commands
- `sid status` (consolidate the duplicate from sid_shell.c)
- `sid mfg`, `sid reinit`, `sid lora`, `sid ble`, `sid reset`
- `sid ota status/abort/delta_test`
- `app <cmd>` routing to app callback
- This replaces both the shell code in app.c AND the duplicate in sid_shell.c

### `sidewalk_dispatch.c` (~150 lines) — Sidewalk event glue
- `on_sidewalk_msg_received()`: routes OTA messages (0x20) to ota_process_msg, others to app callback
- `on_sidewalk_status_changed()`: updates TX state, notifies app
- `on_sidewalk_send_ok()`, `on_sidewalk_send_error()`: forward to app callbacks
- BLE GATT authorization handlers

Also delete `sid_shell.c` (253 lines) — its functionality moves into `platform_shell.c`. Resolves the duplicate `cmd_sid_status()` issue.

Reference: `docs/technical-design-rak-firmware.md`, Change 7.

## Dependencies
**Blocked by**: TASK-050 (DONE — platform EVSE shell files deleted)
**Blocks**: none

## Acceptance Criteria
- [x] `app.c` is ≤250 lines and only handles boot sequence + timer
- [x] `platform_shell.c` contains all platform shell commands (no duplicates)
- [x] `sidewalk_dispatch.c` contains all Sidewalk event handlers
- [x] `sid_shell.c` deleted (functionality consolidated into platform_shell.c)
- [x] No duplicate `cmd_sid_status()` implementations
- [ ] Platform build succeeds
- [ ] All shell commands work: `sid status`, `sid mfg`, `sid ota status`, `app evse status`

## Testing Requirements
- [ ] Platform build succeeds
- [ ] Manual verification: all shell commands respond correctly via serial
- [x] Host-side tests pass (no app-layer changes)

## Deliverables
- New: `src/platform_shell.c` (330 lines), `src/sidewalk_dispatch.c` (195 lines)
- New: `include/sidewalk_dispatch.h`
- Modified: `src/app.c` (reduced from 652 to 204 lines)
- Modified: `include/app.h` (added accessor functions)
- Deleted: `src/sid_shell.c` (consolidated)
- Modified: `CMakeLists.txt` (update source list)
