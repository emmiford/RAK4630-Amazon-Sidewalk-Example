# TASK-047: On-device verification — TIME_SYNC, event buffer, and uplink v0x07

**Status**: in progress (2026-02-17, Eero)
**Priority**: P1
**Owner**: Eero
**Branch**: task/047-058-device-verification + task/047-timesync-resync-on-reboot (merged)
**Size**: S (2 points)

## Description
Combined device verification for TASK-033 (TIME_SYNC), TASK-034 (event buffer), and TASK-035 (uplink v0x07). All three features have host-side tests passing but need physical device verification. Requires platform + app rebuild and reflash.

## Dependencies
**Blocked by**: TASK-033 (DONE), TASK-034 (DONE), TASK-035 (DONE) — all deps resolved, ready to go
**Blocks**: none

## Acceptance Criteria
- [x] TIME_SYNC: `sid time` shell command shows synced time after first uplink
- [ ] TIME_SYNC: Clock drift < 10 seconds per day confirmed
- [x] Event buffer: `evse buffer` shell command shows fill level and timestamps
- [x] Event buffer: ACK watermark from TIME_SYNC trims buffer entries (zero-ts entries replaced)
- [x] Uplink v0x08: DynamoDB events include device-side timestamp, charge_allowed, version=8
- [x] Uplink v0x08: 12-byte payload confirmed (`e50804070144096000000000`)
- [x] Backward compat: decode Lambda still handles v0x06/v0x07 (195 Python tests)

## Testing Requirements
- [x] Reflash platform + app with all three features (MFG → platform → app)
- [x] Monitor serial output during TIME_SYNC receipt
- [x] Verify DynamoDB event format matches expected v0x08 schema
- [ ] Simulate state changes and verify event buffer fill/trim cycle
- [x] Fixed: Lambda now force-sends TIME_SYNC when device reports ts=0 (commit 9e0b165)

## Deliverables
- Updated `tests/e2e/RUNBOOK.md`
- `tests/e2e/RESULTS-time-sync-v07.md`
