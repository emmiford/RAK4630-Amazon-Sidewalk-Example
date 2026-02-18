# TASK-075: On-device delay window verification

**Status**: MERGED DONE (2026-02-18, Eero)
**Priority**: P2
**Owner**: Eero
**Size**: S (2 points)

## Summary
Physical device verification of TASK-063 delay window support. All critical
device-side behaviors confirmed: pause on window received, auto-resume on
expiry, and legacy ALLOW cancels active window immediately.

Untested edge cases (low risk):
- epoch=0 pre-sync guard (code path obvious, deferred)
- Scheduler integration (covered by TASK-064)

## Deliverables
- `tests/e2e/RESULTS-task075-delay-window.md`
