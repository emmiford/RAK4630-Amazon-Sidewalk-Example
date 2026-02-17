# TASK-074: Device registry GSI for fleet-scale health queries

**Status**: not started
**Priority**: P3
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
The health digest Lambda (TASK-029) currently scans the device registry table to enumerate all devices. This is fine for <100 devices but won't scale. Add a DynamoDB Global Secondary Index (GSI) on the `status` attribute to support efficient fleet-wide queries without full table scans.

## Dependencies
**Blocked by**: TASK-029 (health digest Lambda must exist first)
**Blocks**: none

## Acceptance Criteria
- [ ] GSI on `status` attribute with `device_id` as sort key
- [ ] Health digest Lambda updated to query GSI instead of scan
- [ ] Terraform plan validates cleanly

## Testing Requirements
- [ ] Terraform plan shows GSI creation
- [ ] Health digest Lambda tests pass with GSI query path

## Deliverables
- `aws/terraform/main.tf`: GSI on device registry table
- `aws/health_digest_lambda.py`: Use GSI query instead of scan
