# TASK-048: On-device verification — commissioning self-test

**Status**: not started
**Priority**: P0
**Owner**: Eero
**Branch**: N/A (device testing, no code changes expected)
**Size**: S (2 points)

## Description
TASK-039 implemented all self-test logic (boot checks, continuous monitoring, `sid selftest` shell command) with 23 C tests and 4 Lambda decoder tests — all passing and merged to main. Two Definition of Done criteria remain that require the physical device:

1. `sid selftest` verified on physical device — Flash current main (platform + app), run `sid selftest` via serial, confirm pass/fail output.
2. Fault flags appear in DynamoDB uplinks — Trigger a fault condition (e.g., simulate clamp mismatch via `evse c` with no load), confirm uplink byte 7 bits 4-7 are set, verify decode Lambda extracts fault fields.

## Dependencies
**Blocked by**: TASK-039 (DONE)
**Blocks**: none

## Acceptance Criteria
- [ ] `sid selftest` shell command runs on device and prints pass/fail for each check
- [ ] Boot self-test runs on power-on (confirm via serial log)
- [ ] Fault flags appear in uplink byte 7 bits 4-7 when a fault condition is active
- [ ] DynamoDB event contains decoded fault fields (`sensor_fault`, `clamp_mismatch`, `interlock_fault`, `selftest_fail`)
- [ ] Fault flags clear when condition resolves (continuous monitoring self-clear)

## Testing Requirements
- [ ] Reflash platform + app from current main
- [ ] Monitor serial output during boot and `sid selftest`
- [ ] Simulate fault conditions (e.g., `evse c` with no load for clamp mismatch)
- [ ] Query DynamoDB for fault flag fields in decoded events

## Deliverables
- `tests/e2e/RESULTS-selftest.md`
- Updated `tests/e2e/RUNBOOK.md`
