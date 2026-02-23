# TASK-111: Fix dashboard detail view — normalize API contract across endpoints

**Status**: merged done (2026-02-22, Eliel+Utz)
**Priority**: P2
**Owner**: Eliel+Utz
**Branch**: `task/111-dashboard-detail-view-fix` (merged, deleted)
**Size**: S (1 point)

## Summary
Detail endpoint (`GET /devices/{id}`) returned raw DynamoDB objects (nested `registry` + `state`) while the list endpoint (`GET /devices`) returned flat, UI-ready dicts. The frontend treated both identically, causing: online status always OFFLINE (never computed), J1772 state showing `[object Object]`, and voltage/current/charge_allowed/RSSI all showing `--`.

Extracted `_is_online()` and `_flatten_device()` shared helpers. Both endpoints now return the same flat device shape. Frontend updated to use correct DynamoDB field names (`current_draw_ma`, `pilot_voltage_mv`) in both views.

## Deliverables
- `aws/dashboard_api_lambda.py` — `_is_online()`, `_flatten_device()` helpers; refactored `get_devices()` and `get_device_detail()`
- `aws/dashboard/index.html` — detail view and fleet list use correct field names
- `aws/tests/test_dashboard_api.py` — updated for new `{device: {...}}` response shape
- All 406 Python tests pass
