# TASK-078: Implement daily aggregation Lambda

**Status**: not started
**Priority**: P3
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
Implement the daily aggregation Lambda defined in `docs/data-retention.md` §1.2. Triggered by EventBridge on a daily schedule, scans raw telemetry events before they expire (days 85–90 of 90-day TTL window), computes per-device daily summaries, and writes to a `sidecharge-daily-aggregates` DynamoDB table with 3-year TTL. Without this, all data older than 90 days is permanently lost.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Lambda created: EventBridge daily trigger
- [ ] Queries telemetry events for each device for previous day
- [ ] Computes daily aggregates: total kWh, AC compressor hours, EV charge session count, EV charge duration, peak current
- [ ] Writes to `sidecharge-daily-aggregates` table with TTL = 3 years (94,608,000 seconds)
- [ ] DynamoDB table created via Terraform with TTL enabled
- [ ] Handles zero-event days gracefully (no record written)

## Testing Requirements
- [ ] Unit tests: aggregation math, edge cases (no events, partial days)
- [ ] Verify TTL attribute set correctly on aggregate records

## Deliverables
- `aws/aggregation_lambda.py`
- `aws/terraform/aggregation.tf`
- `aws/tests/test_aggregation_lambda.py`
