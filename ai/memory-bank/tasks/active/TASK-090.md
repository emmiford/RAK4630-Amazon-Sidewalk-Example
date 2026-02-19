# TASK-090: Codebase streamlining — test consolidation, DRY constants, LOG macros

**Status**: committed
**Priority**: P3
**Owner**: Eliel, Utz, Eero (joint review)
**Branch**: `task/086-codebase-streamlining`
**Size**: M (3 points)

## Description
Joint review by Eliel, Utz, and Eero identified bloat, overlap, and unnecessary complexity across the firmware and cloud code. Three-phase cleanup:

1. **Phase 1**: Consolidated dual test infrastructure — merged two parallel mock_platform implementations into one, migrated all tests to single CMake build, deleted Grenning Makefile-based system. Net -4200 lines.
2. **Phase 2a**: Extracted shared Python protocol constants (`OTA_CMD_TYPE`, `SIDECHARGE_EPOCH_OFFSET`, `crc32()`) into `aws/protocol_constants.py`. Eliminated duplication across 4 Lambda files.
3. **Phase 3**: Added `LOG_INF`/`LOG_WRN`/`LOG_ERR` macros to `app_platform.h`. Replaced 19 verbose `if (platform) platform->log_*()` patterns. Removed redundant nested platform guards. Extracted `CURRENT_ON_THRESHOLD_MA` into `evse_sensors.h`. Replaced magic `0x02` with `THERMOSTAT_FLAG_COOL`.

Note: Branch uses number 086 due to numbering collision with TASK-086 (CMD_AUTH_KEY). Tracked here as TASK-090.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] Single test build system (CMake-only, 15 test executables)
- [x] Single mock_platform_api implementation (merged from two)
- [x] Grenning Makefile + old mocks deleted
- [x] CI `test-c-grenning` job removed
- [x] Shared Python protocol constants in `aws/protocol_constants.py`
- [x] LOG_INF/LOG_WRN/LOG_ERR macros in `app_platform.h`
- [x] Redundant `if(platform)` guards cleaned up (32 → 13)
- [x] `CURRENT_ON_THRESHOLD_MA` extracted to `evse_sensors.h`
- [x] Magic `0x02` replaced with `THERMOSTAT_FLAG_COOL`
- [x] All 15 C tests pass, all 287 Python tests pass
- [x] CLAUDE.md updated with new test command

## Deliverables
- `tests/CMakeLists.txt` — consolidated build config
- `tests/mocks/mock_platform_api.h/c` — unified mock
- `aws/protocol_constants.py` — shared constants
- `app_platform.h` — LOG macros
- 45 files changed, +949 / -1227 (net -278 lines)
