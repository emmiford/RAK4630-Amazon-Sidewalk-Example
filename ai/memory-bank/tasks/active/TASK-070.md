# TASK-070: Production heartbeat interval (60s → 15min)

**Status**: not started
**Priority**: P2
**Owner**: —
**Branch**: —
**Size**: XS (1 point)

## Description

PRD §3.2 specifies a 15-minute heartbeat interval for production, but the current
firmware uses 60s (`HEARTBEAT_INTERVAL_MS` in `app_entry.c`). The 60s interval is
appropriate for development/testing but wastes Sidewalk airtime in production (LoRa
duty cycle limits).

This is a one-line constant change, but it affects test expectations and cloud-side
offline detection thresholds (TASK-029).

## Dependencies

**Blocked by**: none
**Blocks**: none (but coordinate with TASK-029 offline detection thresholds)

## Acceptance Criteria

- [ ] `HEARTBEAT_INTERVAL_MS` changed from 60000 to 900000 (15 min)
- [ ] Or: make it configurable via a build flag (`-DHEARTBEAT_INTERVAL_MS=900000`)
- [ ] Cloud offline detection threshold updated accordingly (>2x heartbeat = 30+ min)
- [ ] Unit tests updated for new interval

## Deliverables

- Modified `app_entry.c` — heartbeat interval constant
- Updated tests if any assert on the 60s value
