# TASK-078: Implement daily aggregation Lambda

**Status**: merged done (2026-02-19, Eliel)
**Priority**: P3
**Owner**: Eliel
**Branch**: `task/078-daily-aggregation`
**Size**: M (3 points)

## Description
Daily aggregation Lambda — EventBridge trigger at 02:00 UTC, queries previous day's telemetry per device, computes energy/fault/availability metrics, writes to `sidecharge-daily-aggregates` DynamoDB table with 3-year TTL.

## Acceptance Criteria
- [x] Lambda created: EventBridge daily trigger at 02:00 UTC
- [x] Queries telemetry events for each device for previous day
- [x] Computes daily aggregates: total kWh, charge sessions/duration, peak current, AC compressor minutes
- [x] Computes fault aggregates: per-type fault counts + selftest_passed boolean
- [x] Computes availability: event_count, availability_pct, longest_gap_minutes
- [x] Writes to `sidecharge-daily-aggregates` table with TTL = 3 years
- [x] DynamoDB table created via Terraform with TTL enabled
- [x] Handles zero-event days gracefully (no record written)
- [x] Float→Decimal conversion for DynamoDB compatibility
- [x] Unit tests: 39 tests across 11 test classes
- [x] `docs/data-retention.md` updated with aggregate field reference
- [x] CloudWatch PII audit entry added

## Deliverables
- `aws/aggregation_lambda.py`
- `aws/terraform/aggregation.tf`
- `aws/terraform/variables.tf` (aggregates_table_name)
- `aws/tests/test_aggregation_lambda.py` (39 tests)
- `docs/data-retention.md` (updated §1.2, §1.3, §1.4, §4, §5)
