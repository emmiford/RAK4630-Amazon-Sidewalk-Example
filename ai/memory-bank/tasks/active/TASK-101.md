# TASK-101: Build Version Tracking and Release Tooling

**Status**: committed (2026-02-21, Eliel code / Pam+Utz docs)
**Priority**: P2
**Owner**: Eliel (code), Pam+Utz (docs)
**Branch**: `task/101-build-version-code` (code, 6 commits), `task/101-build-version-docs` (docs, 2 commits)
**Size**: M (3 points)

## Description

Add firmware build version tracking across platform and app images, rename `EVSE_VERSION` → `PAYLOAD_VERSION`, bump uplink to v0x0A (15 bytes) with build versions, restructure CLI from monolithic `ota_deploy.py` to `firmware.py` + `release.py` + `ota.py`.

Four version concepts:
- `APP_BUILD_VERSION` — app firmware release number (from `VERSION` file, 0=dev)
- `PLATFORM_BUILD_VERSION` — platform firmware release number (from `PLATFORM_VERSION` file, 0=dev)
- `APP_CALLBACK_VERSION` — ABI version (bumped 3→4 for `build_version` field in callback struct)
- `PAYLOAD_VERSION` (was `EVSE_VERSION`) — uplink byte layout schema (bumped 0x09→0x0A)

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria — Code branch
- [x] `VERSION` and `PLATFORM_VERSION` files created (CMake reads them)
- [x] `#ifndef` guards in `platform_api.h` (CMake overrides defaults)
- [x] `EVSE_VERSION` renamed to `PAYLOAD_VERSION` across entire codebase
- [x] Uplink v0x0A: 15 bytes with app + platform build versions (bytes 13-14)
- [x] `APP_CALLBACK_VERSION` bumped to 4, `build_version` field in `app_callbacks`
- [x] `sid status` reads app version at runtime (v4 callback), platform at compile-time
- [x] Diagnostics response expanded to 15 bytes (app + platform build versions)
- [x] CLI restructure: `firmware.py` + `release.py` + `ota.py`
- [x] `ota_deploy.py` converted to deprecation wrapper with re-exports
- [x] Lambda v0x0A decoder with backwards-compatible v0x09 support
- [x] 15/15 C tests pass
- [x] 333/333 Python tests pass

## Acceptance Criteria — Docs branch
- [x] TDD section 2.4 updated: four-version scheme, VERSION files, git tagging
- [x] TDD section 3.1 updated: v0x0A payload with build version bytes
- [x] TDD section 10.6 added: CLI tools (firmware.py subcommands)
- [x] TDD shell command docs updated: new `sid status` format
- [x] TASK-101 file and INDEX.md updated

## Not yet done (post-merge)
- [ ] Build platform + app firmware
- [ ] Flash device via USB and verify `sid status` output
- [ ] Push to origin

## Deliverables
- `app/rak4631_evse_monitor/VERSION` — app build version (0)
- `app/rak4631_evse_monitor/PLATFORM_VERSION` — platform build version (0)
- `app/rak4631_evse_monitor/include/platform_api.h` — `#ifndef` guards, `PLATFORM_BUILD_VERSION`, ABI v4
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c` — `PAYLOAD_VERSION` 0x0A, 15-byte payload
- `app/rak4631_evse_monitor/src/platform_shell.c` — runtime app version in `sid status`
- `aws/firmware.py` — unified CLI entry point
- `aws/release.py` — version patching, git tagging, build/flash
- `aws/ota.py` — S3/DynamoDB, delta OTA, baseline, monitoring
- `docs/technical-design.md` — versioning convention, v0x0A, CLI docs
