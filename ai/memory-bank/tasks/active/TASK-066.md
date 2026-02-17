# TASK-066: Button re-test clears FAULT_SELFTEST on all-pass

**Status**: not started
**Priority**: P1
**Owner**: —
**Branch**: —
**Size**: S (1 point)

## Description
When the installer triggers the on-demand self-test (5-press button), and all 5
checks pass, clear the FAULT_SELFTEST (0x80) flag. Currently `selftest_boot()`
only sets the flag on failure and never clears it — a prior boot failure stays
latched even if the re-test passes.

This is the production recovery path for FAULT_SELFTEST. Without it, the only
way to clear a latched boot fault is to power-cycle the device or push an OTA
update (both trigger a reboot which resets RAM fault flags).

**Code change**: In `selftest_trigger.c::start_selftest()`, after calling
`selftest_boot(&result)`, if `result.all_pass` is true, clear FAULT_SELFTEST:
```c
if (result.all_pass) {
    /* Clear latched boot fault — installer verified fix in field */
    extern uint8_t selftest_get_fault_flags(void);
    /* Need a new function or direct access to clear just FAULT_SELFTEST */
}
```

Options:
1. Add `selftest_clear_boot_fault(void)` that does `fault_flags &= ~FAULT_SELFTEST`
2. Have `selftest_boot()` itself clear 0x80 on all-pass (changes boot behavior too — may be preferred)

Recommend option 2: `selftest_boot()` should clear 0x80 on pass. This means a
re-run at boot (after power cycle) AND a re-run via button both clear the flag
on success. Simpler, consistent.

## Dependencies
**Blocked by**: TASK-068 (spec — was TASK-065/Pam, renumbered)
**Blocks**: none

## Acceptance Criteria
- [ ] `selftest_boot()` clears FAULT_SELFTEST (0x80) when all 5 checks pass
- [ ] Button-triggered re-test that passes sends a clean uplink (0x80 cleared)
- [ ] Boot self-test that passes does NOT have a stale 0x80 from a prior run
- [ ] Unit test: fail boot → FAULT_SELFTEST set → pass boot → FAULT_SELFTEST cleared
- [ ] Unit test: fail boot → button trigger passes → FAULT_SELFTEST cleared
- [ ] Existing tests still pass (no regressions)

## Testing Requirements
- [ ] New unit test in test_app.c or test_selftest_trigger.c
- [ ] `make -C tests/ clean test` passes
- [ ] Verify on hardware during TASK-048

## Deliverables
- selftest.c (clear 0x80 on all-pass in `selftest_boot()`)
- test_app.c or test_selftest_trigger.c (new unit tests)
