# TASK-092: BUG: J1772 state enum mismatch between firmware and Lambda

**Status**: merged done (2026-02-19, Eliel)
**Priority**: P1
**Branch**: `task/092-j1772-state-enum-fix`
**Size**: S (1 point)

## Summary
Fixed Lambda `J1772_STATES` mapping to match firmware enum (`A=0, B=1, ..., UNKNOWN=6`).
Updated 3 Python tests that used wrong state codes. All 326 Python tests and 15 C tests pass.

## Files Changed
- `aws/decode_evse_lambda.py` — corrected J1772_STATES map
- `aws/tests/test_decode_evse.py` — fixed state code bytes in 2 tests
- `aws/tests/test_lambda_chain.py` — fixed state code byte in integration test
