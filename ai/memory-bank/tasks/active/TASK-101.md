# TASK-101: Add APP_BUILD_VERSION to sid status

**Status**: in progress (2026-02-20, Eliel code / Pam+Utz docs)
**Priority**: P2
**Owner**: Eliel (code), Pam+Utz (docs)
**Branch**: `task/101-build-version` (code), `task/101-build-version-docs` (docs)
**Size**: S (2 points)

## Description

Add a simple `APP_BUILD_VERSION` (uint8_t, 1-255) to the firmware so that each OTA
deploy has a unique, queryable build number. This is independent of `EVSE_VERSION`
(wire format) and `APP_CALLBACK_VERSION` (ABI version).

The build version is:
- Defined as `#define APP_BUILD_VERSION N` in `platform_api.h`
- Displayed in `sid status` as `App build: vN (API vN, payload v0xNN)`
- Printed in the boot log during `discover_app_image()`
- Returned in diagnostics response byte 13 (0xE6 payload) for cloud querying
- Incremented by `ota_deploy.py --version N` before each OTA deploy
- NOT included in the regular 13-byte EVSE uplink (diagnostics-only)

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] `APP_BUILD_VERSION` defined in `platform_api.h` (code branch)
- [ ] `sid status` displays build version alongside API and payload versions (code branch)
- [ ] Boot log prints build version (code branch)
- [ ] Diagnostics response byte 13 carries `APP_BUILD_VERSION` (code branch)
- [ ] `ota_deploy.py --version N` patches the define before building (code branch)
- [ ] `docs/technical-design.md` documents the versioning convention (docs branch)
- [ ] TASK-101 file created and INDEX.md updated (docs branch)

## Testing Requirements
- [ ] Existing unit tests still pass after adding the define
- [ ] `sid status` output verified on device (manual)
- [ ] Diagnostics response byte 13 verified via 0x40 request (manual)

## Deliverables
- `platform_api.h` — add `APP_BUILD_VERSION` define
- `platform_shell.c` or `app.c` — display in `sid status`
- `app_diag.c` or equivalent — populate byte 13 in diagnostics response
- `ota_deploy.py` — `--version` flag patches the define
- `docs/technical-design.md` — document versioning convention (section 2.4)
