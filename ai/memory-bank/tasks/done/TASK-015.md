# TASK-015: Remove dead sid_demo_parser code

**Status**: MERGED DONE (2026-02-11, Claude)
**Branch**: `main`

## Summary
Deleted 5 files (~1,600 lines) from `ext/`, removed `ext/` directory entirely. Removed source entry and include path from `CMakeLists.txt`. Grep confirmed zero references in app code. All 9 C test suites pass.

## Deliverables
- Clean codebase (5 files, ~1,600 lines removed)
