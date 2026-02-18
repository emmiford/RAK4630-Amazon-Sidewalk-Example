# TASK-077: Implement deletion Lambda (PII + telemetry cleanup)

**Status**: not started
**Priority**: P3
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
Implement the customer data deletion Lambda defined in `docs/data-retention.md` §2.4. Triggered daily by EventBridge, queries device registry for devices with `status=returned` past the 30-day grace period, removes PII attributes, and batch-deletes all associated telemetry and aggregate records. Required to fulfill CCPA Right to Delete. See also `docs/privacy-governance.md` §4.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Lambda created: EventBridge daily trigger → queries registry for `status=returned` + `return_date + 30d < now`
- [ ] PII attributes removed from registry (owner_name, owner_email, install_address, install_lat, install_lon, meter_number, installer_name)
- [ ] All telemetry events for device_id batch-deleted from events table
- [ ] All daily aggregates for device_id batch-deleted (when aggregates table exists)
- [ ] `deleted_at` timestamp set on registry record
- [ ] Audit log: logs device_id only (no PII) for deletion audit trail
- [ ] Terraform resource added (Lambda + IAM + EventBridge rule)

## Testing Requirements
- [ ] Unit tests: mock DynamoDB deletion flow
- [ ] Integration test: verify PII attributes removed, telemetry deleted, hardware record retained

## Deliverables
- `aws/deletion_lambda.py`
- `aws/terraform/deletion.tf`
- `aws/tests/test_deletion_lambda.py`
