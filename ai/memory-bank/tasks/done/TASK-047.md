# TASK-047: On-device verification — TIME_SYNC, event buffer, and uplink v0x08

**Status**: MERGED DONE (2026-02-17, Eero)
**Priority**: P1
**Owner**: Eero
**Size**: S (2 points)

## Summary
Combined device verification for TASK-033 (TIME_SYNC), TASK-034 (event buffer), and TASK-035 (uplink v0x07→v0x08). E2E results: 25/25 testable items PASS. 3 TIME_SYNC downlink items blocked by PSA -149 (TASK-023) — tracked there.

## Key Results
- Uplink v0x08 12-byte payload verified on device and in DynamoDB (schema 2.1)
- Shell commands `app sid time` and `app evse buffer` confirmed working
- Backward compat with v0x06/v0x07 verified (195 Python tests)
- E2E results: `tests/e2e/RESULTS-task047-time-sync-v08.md`
