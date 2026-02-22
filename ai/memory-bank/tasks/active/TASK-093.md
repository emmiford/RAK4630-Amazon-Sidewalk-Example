# TASK-093: Clean up stale IoT rules and old Lambdas

**Status**: in progress
**Priority**: P2
**Owner**: Oliver
**Branch**: (done via CLI, no branch needed)
**Size**: S (1 point)

## Description
The AWS account has 4 stale IoT topic rules and 2 old Lambda functions left over from pre-Terraform manual deployments. They create duplicate and incorrectly-decoded DynamoDB records alongside the Terraform-managed `uplink-decoder` Lambda.

### Stale IoT rules
| Rule | Topic | Target | Issue |
|------|-------|--------|-------|
| `sidewalk_evse_decoded` | `sidewalk/evse_data` | `decode-evse-sidewalk` (Feb 4) | Old Lambda, can't decode v0x09, produces `unknown_sid_demo` |
| `sidewalk_evse_to_dynamodb` | `sidewalk/evse_data` | DynamoDB direct | Writes raw `sidewalk_raw` records (schema 1.0), duplicates |
| `evse_sidewalk_telemetry` | `$aws/things/+/sidewalk/uplink` | `evse-monitor-decode-telemetry` (Feb 3) | Old Lambda, pre-dates all current features |
| `sidewalk_v1_sidewalk_events_to_dynamodb_v2` | `sidewalk/#` | DynamoDB direct | v1 pipeline, writes duplicate raw records |

### Old Lambdas to delete
- `decode-evse-sidewalk` (last modified 2026-02-04)
- `evse-monitor-decode-telemetry` (last modified 2026-02-03)

### Terraform-managed rule (keep)
- `evse_sidewalk_rule` → `uplink-decoder` — this is the correct pipeline

### Approach
1. Disable the 4 stale IoT rules (or delete via AWS console/CLI)
2. Delete the 2 old Lambda functions
3. Optionally import `evse_sidewalk_rule` into Terraform if not already managed
4. Verify only one DynamoDB record per uplink after cleanup

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] Old Lambda functions deleted (7 total: 5 sidewalk-v1 + decode-evse-sidewalk + evse-monitor-decode-telemetry)
- [x] 2 stale IoT rules deleted (sidewalk_evse_decoded, evse_sidewalk_telemetry)
- [ ] 2 remaining stale IoT rules: `sidewalk_evse_to_dynamodb`, `sidewalk_v1_sidewalk_events_to_dynamodb_v2`
- [ ] `sidewalk_debug_log` — evaluate keep/delete
- [ ] Only one DynamoDB record per Sidewalk uplink (from `uplink-decoder`)
- [ ] Verified with live device uplink: single correctly-decoded record

## Testing Requirements
- [ ] Trigger an uplink and confirm single DynamoDB entry with schema_version 2.1

## Deliverables
- AWS cleanup (IoT rules + old Lambdas)
- Optionally: Terraform import of the IoT rule
