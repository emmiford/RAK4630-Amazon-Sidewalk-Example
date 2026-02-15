# TASK-007: Create E2E test plan for device-to-cloud round-trip

**Status**: MERGED DONE (2026-02-11)
**Branch**: `main`

## Summary
E2E runbook at `tests/e2e/RUNBOOK.md`. Covers boot/connect, telemetry flow, charge control downlink, OTA update, and sensor simulation. Manual checklist format with shell + AWS CLI verification commands. Recovery path (power cycle during OTA apply) not covered (see TASK-013).

## Deliverables
- `tests/e2e/RUNBOOK.md`
