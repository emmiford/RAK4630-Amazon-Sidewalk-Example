# TASK-101: Build Version Tracking and Release Tooling

**Status**: merged done (2026-02-21, Eliel code / Pam+Utz docs)
**Priority**: P2
**Owner**: Eliel (code), Pam+Utz (docs)
**Branch**: `task/101-build-version-code` (8 commits), `task/101-build-version-docs` (2 commits)

## Summary

Added firmware build version tracking across platform and app images. Four version concepts: `APP_BUILD_VERSION` (from `BUILD_VERSION` file), `PLATFORM_BUILD_VERSION` (from `PLATFORM_BUILD_VERSION` file), `APP_CALLBACK_VERSION` (ABI v4), `PAYLOAD_VERSION` (wire format v0x0A, was `EVSE_VERSION`). Uplink payload expanded from 13 to 15 bytes with build versions in bytes 13-14. CLI restructured from monolithic `ota_deploy.py` to `firmware.py` + `release.py` + `ota.py`. Zephyr-format `VERSION` file added for `app_version.h` generation (separate from our `BUILD_VERSION` file).

## Deliverables
- `app/rak4631_evse_monitor/BUILD_VERSION` — app build version (0)
- `app/rak4631_evse_monitor/PLATFORM_BUILD_VERSION` — platform build version (0)
- `app/rak4631_evse_monitor/VERSION` — Zephyr-format version file (for `app_version.h` generation)
- `app/rak4631_evse_monitor/include/platform_api.h` — `#ifndef` guards, `PLATFORM_BUILD_VERSION`, ABI v4
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c` — `PAYLOAD_VERSION` 0x0A, 15-byte payload
- `app/rak4631_evse_monitor/src/platform_shell.c` — runtime app version in `sid status`
- `aws/firmware.py` — unified CLI entry point
- `aws/release.py` — version patching, git tagging, build/flash
- `aws/ota.py` — S3/DynamoDB, delta OTA, baseline, monitoring
- `docs/technical-design.md` — versioning convention, v0x0A, CLI docs

## Note
Device flashed but serial verification pending — USB CDC port blocked after platform reflash. Power cycle device and run `sid status` to confirm: `Platform: v0  App: v0  (API v4)`.
