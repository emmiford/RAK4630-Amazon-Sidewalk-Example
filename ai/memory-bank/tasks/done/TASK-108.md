# TASK-108: Dashboard event table missing timestamps

**Status**: merged done
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/106-dashboard-migration`
**Size**: XS (0.5 points)
**Date completed**: 2026-02-22

## Summary
Fixed `ev.timestamp || ev.time` → `ev.timestamp_mt || ev.timestamp || ev.time` in `aws/dashboard/index.html`. Also fixed online badge using `d.status` (always `"unknown"`) instead of `d.online` boolean. Added 72h time window option.

## Deliverables
- Fix in `aws/dashboard/index.html` (3 commits: `e8a798a`, `8d5dbf7`, `9e33b80`)
