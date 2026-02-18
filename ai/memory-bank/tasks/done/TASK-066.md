# TASK-066: Button re-test clears FAULT_SELFTEST on all-pass

**Status**: merged done (2026-02-17, Eliel)
**Priority**: P1
**Branch**: `task/066-selftest-clear-fault` (merged to main)
**Size**: S (1 point)

## Summary
`selftest_boot()` now clears FAULT_SELFTEST (0x80) when all checks pass,
enabling the 5-press button re-test as the production recovery path for
latched boot faults.

## Deliverables
- `selftest.c` — added `else { fault_flags &= ~FAULT_SELFTEST; }` in `selftest_boot()`
- `test_app.c` — `test_selftest_boot_flag_clears_on_retest`, `test_selftest_boot_no_stale_fault_on_pass`
- `test_selftest_trigger.c` — `test_button_retest_clears_fault_on_pass`
- 97/97 host tests + 18/18 Unity trigger tests pass
