# TASK-005: Add OTA recovery path host-side tests

**Status**: MERGED DONE (2026-02-11, Eero)
**Branch**: `feature/testing-pyramid`

## Summary
16 tests using Unity/CMake framework with RAM-backed mock flash (400KB). Mock Zephyr headers at `tests/mocks/zephyr/`. Key finding: `clear_metadata()` page-aligned erase at 0xCFF00 extends to 0xD0FFF, partially erasing first page of staging area.

## Deliverables
- `tests/app/test_ota_recovery.c` (16 tests)
- `tests/mocks/mock_flash.c`
- `tests/mocks/zephyr/` (7 mock headers)
- Updated `tests/CMakeLists.txt`
