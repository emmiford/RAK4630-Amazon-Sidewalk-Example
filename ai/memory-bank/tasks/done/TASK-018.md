# TASK-018: Add old Grenning tests to CI

**Status**: MERGED DONE (2026-02-11, Eero)
**Branch**: `feature/testing-pyramid`

## Summary
Added `test-c-grenning` job to `.github/workflows/ci.yml`. Runs `make -C app/rak4631_evse_monitor/tests clean test` as a separate CI job. All 4 CI jobs passing.

## Deliverables
- Updated `.github/workflows/ci.yml`
