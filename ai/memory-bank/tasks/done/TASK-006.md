# TASK-006: Add decode Lambda unit tests

**Status**: MERGED DONE (2026-02-11)
**Branch**: `feature/testing-pyramid`

## Summary
20 tests covering raw EVSE payload decoding (9 tests), OTA uplink decoding (6 tests), full base64 decode pipeline (4 tests), and lambda handler integration (2 tests). Legacy sid_demo format decoding not explicitly tested (low priority fallback path).

## Deliverables
- `aws/tests/test_decode_evse.py` (20 tests)
