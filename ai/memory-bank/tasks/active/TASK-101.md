# TASK-101: Build Version Tracking and Release Tooling

**Status**: in progress (2026-02-21, Eliel code / Pam+Utz docs)
**Priority**: P2
**Owner**: Eliel (code), Pam+Utz (docs)
**Branch**: `task/101-build-version-code` (code), `task/101-build-version-docs` (docs)
**Size**: M (3 points)

## Description

Add build version tracking to the firmware so that each OTA deploy has a unique,
queryable build number visible in uplinks, shell output, and diagnostics. Restructure
the deployment CLI from a monolithic `ota_deploy.py` into modular tools with proper
release management (version patching, git tagging, build invocation).

### Version Scheme (four independent versions)

| Name | Type | What it means | When it changes |
|------|------|---------------|-----------------|
| `PAYLOAD_VERSION` (was `EVSE_VERSION`) | uint8_t, hex (0x0A) | Uplink byte layout schema | Only when payload byte layout changes |
| `APP_BUILD_VERSION` | uint8_t (1-255, 0=dev) | Which app firmware build is running | Every tagged release (via `release.py`) |
| `PLATFORM_BUILD_VERSION` | uint8_t (1-255, 0=dev) | Which platform firmware build is running | Every platform release (rare, USB-only) |
| `APP_CALLBACK_VERSION` | uint32_t (3) | Platform<>app ABI table layout | Only when function pointer table changes (ADR-001) |

### Key design decisions

- **VERSION files** as single source of truth (not C header #defines)
- **CMake reads VERSION files** at build time, passes `-D` flags
- **platform_api.h uses #ifndef** so CMake flags override default of 0
- **Uplink v0x0A** grows from 13 to 15 bytes (bytes 13-14 = build versions)
- **Runtime app version**: platform reads from app image, not compile-time constant
- **Git tags**: annotated `app-vN` tags, enforced by release script

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria

### Code branch (`task/101-build-version-code`)
- [ ] `app/rak4631_evse_monitor/VERSION` file (app build version, single integer)
- [ ] `app/rak4631_evse_monitor/PLATFORM_VERSION` file (platform build version)
- [ ] CMake reads VERSION files and passes `-DAPP_BUILD_VERSION=N` / `-DPLATFORM_BUILD_VERSION=N`
- [ ] `platform_api.h` uses `#ifndef` guards (CMake flags override default 0)
- [ ] `EVSE_VERSION` renamed to `PAYLOAD_VERSION` in `evse_payload.h`
- [ ] Uplink payload v0x0A: bytes 13-14 carry app + platform build versions
- [ ] `sid status` displays: `Platform: vN  App: vN  (API vN, payload v0xNN)`
- [ ] Boot log prints both build versions during `discover_app_image()`
- [ ] Diagnostics response byte 13 carries `APP_BUILD_VERSION` (0xE6 payload)
- [ ] App build version read at runtime (not compile-time) so OTA updates reflect correctly
- [ ] `firmware.py` CLI entry point with subcommand routing
- [ ] `release.py` patches VERSION, commits, tags `app-vN`, builds
- [ ] `ota.py` handles S3 upload, OTA session management, monitoring
- [ ] Release script refuses dirty working trees and duplicate tags
- [ ] Dev builds (v0) immediately identifiable on shell

### Docs branch (`task/101-build-version-docs`)
- [x] TDD section 2.4: four-version scheme with PAYLOAD_VERSION rename
- [x] TDD section 2.4: VERSION file convention and CMake integration
- [x] TDD section 2.4: git tagging convention
- [x] TDD section 3.1: uplink v0x0A format (15 bytes with build versions)
- [x] TDD section 3.3: legacy table updated (v0x09 moved to legacy)
- [x] TDD section 10.6: CLI tool restructure (firmware.py + release.py + ota.py)
- [x] TDD section 11.3: sid status output format updated
- [x] TASK-101 acceptance criteria expanded to reflect full scope

## Testing Requirements
- [ ] Existing unit tests still pass after PAYLOAD_VERSION rename
- [ ] New tests for VERSION file reading in CMake
- [ ] `sid status` output verified on device (manual)
- [ ] Diagnostics response byte 13 verified via 0x40 request (manual)
- [ ] Uplink v0x0A decoded correctly by Lambda (Python tests)
- [ ] `firmware release` and `firmware deploy` end-to-end (manual)

## Deliverables
- `app/rak4631_evse_monitor/VERSION` — app build version file
- `app/rak4631_evse_monitor/PLATFORM_VERSION` — platform build version file
- `evse_payload.h` — PAYLOAD_VERSION rename, v0x0A payload with build versions
- `platform_api.h` — `#ifndef` guards for build version defines
- `platform_shell.c` / `app.c` — `sid status` format: `Platform: vN  App: vN  (API vN, payload v0xNN)`
- `app_diag.c` or equivalent — populate byte 13 in diagnostics response
- `aws/firmware.py` — CLI entry point
- `aws/release.py` — version patching, git tagging, build
- `aws/ota.py` — OTA deployment and monitoring
- `docs/technical-design.md` — all documentation updates
