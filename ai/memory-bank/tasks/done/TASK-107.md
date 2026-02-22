# TASK-107: Device timestamp as DynamoDB sort key (ADR-007)

**Status**: merged done (2026-02-22, Eliel)
**Priority**: P2
**Branch**: `task/107-device-timestamp-sk` (merged + cleaned up)

## Summary
Buffered EVSE events that arrive hours late now sort by when they happened, not when AWS received them. `compute_event_timestamp_ms()` uses device time + cloud ms fraction as the SK for synced EVSE telemetry; everything else falls back to cloud receive time. New attributes `timestamp_source` and `cloud_received_mt` tag every record.

## Deliverables
- `aws/decode_evse_lambda.py` — `compute_event_timestamp_ms()` helper, updated `lambda_handler` and `store_transition_event`
- `aws/charge_scheduler_lambda.py` — `log_command_event` adds `timestamp_source` + `cloud_received_mt`
- `aws/tests/test_decode_evse.py` — 12 new tests (6 unit + 6 integration)
- `aws/tests/test_charge_scheduler.py` — 4 new assertions
- `docs/adr/007-device-timestamp-as-sort-key.md` — ADR-007
- `docs/adr/README.md` — index updated
- Deployed via Terraform (uplink-decoder + charge-scheduler Lambdas)
