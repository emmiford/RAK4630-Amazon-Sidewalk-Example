# TASK-047: On-device verification — TIME_SYNC, event buffer, and uplink v0x07

**Status**: not started
**Priority**: P1
**Owner**: —
**Branch**: N/A (device testing, no code changes expected)
**Size**: S (2 points)

## Description
Combined device verification for TASK-033 (TIME_SYNC), TASK-034 (event buffer), and TASK-035 (uplink v0x07). All three features have host-side tests passing but need physical device verification. Requires platform + app rebuild and reflash.

## Dependencies
**Blocked by**: TASK-033 (DONE), TASK-034 (DONE), TASK-035 (DONE) — all deps resolved, ready to go
**Blocks**: none

## Acceptance Criteria
- [ ] TIME_SYNC: `sid time` shell command shows synced time after first uplink
- [ ] TIME_SYNC: Clock drift < 10 seconds per day confirmed
- [ ] Event buffer: `evse buffer` shell command shows fill level and timestamps
- [ ] Event buffer: ACK watermark from TIME_SYNC trims buffer entries
- [ ] Uplink v0x07: DynamoDB events include device-side timestamp, charge_allowed, version=0x07
- [ ] Uplink v0x07: 12-byte payload confirmed in CloudWatch logs
- [ ] Backward compat: decode Lambda still handles v0x06

## Testing Requirements
- [ ] Reflash platform + app with all three features
- [ ] Monitor serial output during TIME_SYNC receipt
- [ ] Verify DynamoDB event format matches expected v0x07 schema
- [ ] Simulate state changes and verify event buffer fill/trim cycle

## Deliverables
- Updated `tests/e2e/RUNBOOK.md`
- `tests/e2e/RESULTS-time-sync-v07.md`
