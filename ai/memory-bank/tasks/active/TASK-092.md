# TASK-092: BUG: J1772 state enum mismatch between firmware and Lambda

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
The firmware J1772 state enum starts at `J1772_STATE_A = 0` (`evse_sensors.h:16`) but the decode Lambda maps `0: 'UNKNOWN'` (`decode_evse_lambda.py:85`). Every J1772 state decoded in production is shifted by one position:

| Firmware sends | Lambda decodes as | Correct meaning |
|---|---|---|
| 0 (State A) | UNKNOWN | No vehicle (+12V) |
| 1 (State B) | A | Vehicle connected (+9V) |
| 2 (State C) | B | Charging (+6V) |
| 6 (UNKNOWN) | F | Unknown state |

This means all DynamoDB telemetry records have incorrect J1772 state labels. The Python test suite masks the bug because it tests against the Lambda's mapping, not the firmware's encoding.

### Root cause
The firmware enum was defined without an explicit UNKNOWN=0 sentinel, using `A=0` as the natural first state. The Lambda map was written with `0: 'UNKNOWN'` assuming a sentinel-first convention.

### Fix options
1. **Fix Lambda mapping** (preferred): Change `decode_evse_lambda.py` to `{0: 'A', 1: 'B', ..., 6: 'UNKNOWN'}` matching firmware
2. **Fix firmware enum**: Add `J1772_STATE_UNKNOWN = 0` and shift all others — but this changes the wire format and requires an OTA

Option 1 is the right fix: change the Lambda, update the Python tests, and backfill any DynamoDB records if needed.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Lambda J1772 state map matches firmware enum exactly
- [ ] Python tests updated to match corrected mapping
- [ ] All existing Python tests pass
- [ ] C unit tests still pass (no firmware change needed for Option 1)
- [ ] Verified with live device uplink: State A → "A" in DynamoDB, State C → "C"

## Testing Requirements
- [ ] `python3 -m pytest aws/tests/ -v` — all pass
- [ ] `cmake ... && ctest ...` — all pass (no change expected)

## Deliverables
- `aws/decode_evse_lambda.py` — corrected J1772_STATE_MAP
- `aws/tests/` — updated test expectations
