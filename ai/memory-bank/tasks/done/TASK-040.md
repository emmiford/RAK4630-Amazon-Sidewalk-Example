# TASK-040: Production self-test trigger — 5-press button with LED blink codes

**Status**: merged done (2026-02-14, Eero + Pam)
**Branch**: `task/040-prod-selftest-trigger`
**Merged**: 6c67b8e — PRD updated with 500ms polling limitation, TASK-048/049 traceability

## Summary
5-press detection within 5-second window on Charge Now button GPIO (pin 3). Triggers full boot self-test cycle. LED blink-code output: green = passed, red = failed. 0 failed = green-only (no red). Special uplink with FAULT_SELFTEST flag on failure. Single-press Charge Now not affected. 17 C unit tests, all passing.

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/selftest_trigger.c`
- `app/rak4631_evse_monitor/include/selftest_trigger.h`
- `tests/app/test_selftest_trigger.c` (17 tests)
- `tests/mocks/mock_platform_api.{h,c}` (enhanced)
