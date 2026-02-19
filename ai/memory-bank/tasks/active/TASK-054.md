# TASK-054: Replace per-module API setters with shared platform pointer

**Status**: committed
**Priority**: P2
**Owner**: Eliel
**Branch**: task/054-shared-platform-pointer
**Size**: M (3 points)

## Description
Replaced 13 per-module `*_set_api()` functions with a single shared `const struct platform_api *platform` pointer in `app_platform.c`. All 14 app modules now `#include <app_platform.h>` and use `platform->` directly. Net reduction of 143 lines.

Added `app_tx_init()` to replace the rate-limiter reset that was a side effect of the old `app_tx_set_api()`.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] No `*_set_api()` functions remain in app sources
- [x] No `static const struct platform_api *api` in any app module
- [x] Single `const struct platform_api *platform` pointer in `app_platform.c`
- [x] `app_init()` sets it once
- [x] All 14 app modules use `platform->` for API calls
- [x] App build succeeds (both standalone and host test)
- [x] All 189 tests pass (179 unit + 10 boot path)

## Testing Requirements
- [x] All 189 host-side tests pass
- [x] App build succeeds
- [x] Mock setup: `platform = mock_api()` in tests (129 calls updated)

## Deliverables
- New: `app_platform.h`, `app_platform.c`
- Modified: 14 app modules (remove setter + static, add include, api-> to platform->)
- Modified: `app_entry.c` (remove 13 setter calls, add 1 assignment + app_tx_init())
- Modified: 13 headers (remove _set_api declarations and forward decls)
- Modified: `test_app.c` (129 set_api calls → platform = mock_api())
- Modified: `app_evse/CMakeLists.txt`, `tests/Makefile` (add app_platform.c)
