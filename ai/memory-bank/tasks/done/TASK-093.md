# TASK-093: Clean up stale IoT rules and old Lambdas

**Status**: merged done
**Priority**: P2
**Owner**: Oliver
**Completed**: 2026-02-22

## Summary
Deleted all pre-Terraform AWS resources that were creating duplicate DynamoDB records and wasting invocations.

## What was deleted
**7 old Lambda functions:**
- `sidewalk-v1-archive-device-events-v2`
- `sidewalk-v1-ingest-device-events-v2`
- `sidewalk-v1-debug-query`
- `sidewalk-v1-hvac-energy-calc`
- `sidewalk-v1-energy-aggregate`
- `decode-evse-sidewalk` (Feb 4, triggered by IoT rule on real uplink topic)
- `evse-monitor-decode-telemetry` (Feb 3, triggered by IoT rule on real uplink topic)

**5 stale IoT rules:**
- `sidewalk_evse_decoded` → old Lambda
- `evse_sidewalk_telemetry` → old Lambda (was duplicating real uplinks)
- `sidewalk_evse_to_dynamodb` → DynamoDB direct (schema 1.0 duplicates)
- `sidewalk_v1_sidewalk_events_to_dynamodb_v2` → DynamoDB direct (v1 pipeline)
- `sidewalk_debug_log` → CloudWatch v1 log group

## What remains (Terraform-managed only)
- 1 IoT rule: `evse_sidewalk_rule` → `uplink-decoder`
- 6 Lambdas: `uplink-decoder`, `charge-scheduler`, `ota-sender`, `health-digest`, `daily-aggregation`, `dashboard-api`
