# TASK-004: Add charge scheduler Lambda unit tests

**Status**: MERGED DONE (2026-02-11)
**Branch**: `feature/testing-pyramid`

## Summary
17 tests covering TOU peak detection (8 tests), charge command payload format (3 tests), lambda handler decision logic (3 tests), and MOER integration (3 tests). All passing in CI.

## Deliverables
- `aws/tests/test_charge_scheduler.py` (17 tests)
