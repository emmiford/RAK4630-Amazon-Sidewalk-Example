# TASK-112: Fix dashboard 500 — Lambda response exceeded 6MB payload limit

**Status**: merged done (2026-02-22, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: `task/112-dashboard-payload-limit` (merged + deleted)
**Size**: S (1 point)

## Description
Device detail endpoint (`GET /devices/{id}`) returned unbounded events with full raw DynamoDB objects in each summary. With enough events (especially on 72h window), the JSON response exceeded Lambda's 6MB payload limit — Lambda runtime returned HTTP 413 internally, surfacing as 500 to the browser.

## Deliverables
- Capped event query at 500 events in `get_device_detail()`
- Dropped unused `raw` field from event summaries (frontend never reads it)
- Response size for 72h window: >6MB → ~70KB
- All 30 dashboard API tests pass
- Deployed via `terraform apply`
