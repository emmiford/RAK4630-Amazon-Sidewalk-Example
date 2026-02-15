# TASK-024: Harden API version mismatch to hard stop

**Status**: MERGED DONE (2026-02-11, Claude)
**Branch**: `main`

## Summary
Changed `discover_app_image()` in `app.c` from warning to hard stop on version mismatch. Added `app_reject_reason` tracking for shell diagnostics. Updated `sid status` to show rejection reason. ADR-001 written. KI-001 documented.

## Deliverables
- Modified `app.c` (hard stop on version mismatch)
- `docs/adr/001-version-mismatch-hard-stop.md`
- `docs/known-issues.md` (KI-001)
