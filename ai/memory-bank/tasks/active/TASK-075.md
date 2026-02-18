# TASK-075: On-device delay window verification

**Status**: not started
**Priority**: P2
**Owner**: —
**Branch**: —
**Size**: S (2 points)

## Description
Physical device verification of TASK-063 delay window support. Confirm that
the device correctly receives, stores, and acts on delay window downlinks
sent from the cloud scheduler.

### Why
TASK-063 was developed and unit-tested on the host, but the delay window
code path has never been exercised on real hardware over LoRa. This task
verifies the end-to-end flow: cloud sends delay window → device parses it →
device pauses charging → device auto-resumes on expiry.

### Test plan
1. Flash firmware with delay window support (from TASK-063 branch merge)
2. Wait for TIME_SYNC (device must have non-zero epoch)
3. Send a delay window downlink via CLI (`send_sidewalk_msg`) with
   start=now, end=now+120s
4. Verify device pauses charging (`app evse status` shows charge_allowed=false)
5. Wait for window to expire (120s)
6. Verify device resumes charging autonomously (no cloud message needed)
7. Send a delay window, then cancel with legacy allow — verify immediate resume
8. Verify `app evse status` reflects delay window state (if shell output updated)
9. Verify device ignores delay window before TIME_SYNC (cold boot, send window
   before first sync — device should remain in allow state)

## Dependencies
**Blocked by**: none (TASK-063 merged to main 2026-02-17)
**Blocks**: none

## Acceptance Criteria
- [ ] Device pauses charging when active delay window is received
- [ ] Device resumes autonomously when delay window expires
- [ ] Legacy allow command cancels active delay window immediately
- [ ] Device ignores delay window when time is not synced (epoch=0)
- [ ] Scheduler integration: EventBridge-triggered scheduler sends delay window during TOU peak

## Testing Requirements
- [ ] Manual: send delay window via CLI, observe device behavior via serial console
- [ ] Manual: let window expire, confirm auto-resume
- [ ] Manual: send legacy allow to cancel active window
- [ ] Manual: cold boot (no TIME_SYNC), send delay window, confirm ignored

## Deliverables
- Test results documented (serial console logs)
- Any bug fixes discovered during verification
