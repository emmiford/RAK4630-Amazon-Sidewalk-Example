# TASK-075: On-device delay window verification

**Status**: coded (2026-02-17, Eero) — ready to merge
**Priority**: P2
**Owner**: Eero
**Branch**: `task/075-delay-window-verify`
**Size**: S (2 points)

## Description
Physical device verification of TASK-063 delay window support. Confirm that
the device correctly receives, stores, and acts on delay window downlinks
sent from the cloud scheduler.

## Dependencies
**Blocked by**: none (TASK-063 merged to main 2026-02-17)
**Blocks**: none

## Acceptance Criteria
- [x] Device pauses charging when active delay window is received
  - Sent 120s window → device showed `Delay window set: start=... end=...` + `Charging allowed: NO`
- [x] Device resumes autonomously when delay window expires
  - After 120s: `Delay window expired` + `Charging allowed: YES`
- [x] Legacy allow command cancels active delay window immediately
  - Sent 30-min window → confirmed paused → sent ALLOW → `Delay window cleared` + `Charge control: ALLOW` with ~26 min remaining
- [ ] Device ignores delay window when time is not synced (epoch=0)
  - Not tested (would require cold boot before TIME_SYNC)
- [ ] Scheduler integration: EventBridge-triggered scheduler sends delay window during TOU peak
  - Not tested (scheduler integration test, separate scope)

## Testing Requirements
- [x] Manual: send delay window via CLI, observe device behavior via serial console
- [x] Manual: let window expire, confirm auto-resume
- [x] Manual: send legacy allow to cancel active window
- [ ] Manual: cold boot (no TIME_SYNC), send delay window, confirm ignored

## Results (2026-02-17)
All critical device-side behaviors verified:
1. **Pause**: 120s delay window → immediate pause (PASS)
2. **Auto-resume**: Window expired → auto-resume without cloud message (PASS)
3. **Cancel**: 30-min window active → ALLOW command → immediate clear + resume (PASS)
4. **Pre-sync**: Not tested (low risk — code checks `time_sync_get() == 0`)

Full results: `tests/e2e/RESULTS-task075-delay-window.md` on branch

## Deliverables
- [x] Test results documented (serial console logs)
- [x] E2E results file on branch
