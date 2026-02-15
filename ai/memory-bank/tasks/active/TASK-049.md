# TASK-049: Deploy device registry — terraform apply + verify uplink creates entry

**Status**: not started
**Priority**: P0
**Owner**: Eliel
**Branch**: N/A (infrastructure deployment, no code changes expected)
**Size**: XS (1 point)

## Description
TASK-036 merged code for the device registry DynamoDB table, but `terraform apply` has not been run yet. The table `sidecharge-device-registry` does not exist in AWS. This task deploys the infrastructure and verifies the next device uplink auto-provisions a registry entry.

## Dependencies
**Blocked by**: TASK-036 (DONE)
**Blocks**: TASK-037 (utility identification needs registry deployed)

## Acceptance Criteria
- [ ] `terraform apply` succeeds — `sidecharge-device-registry` table created with both GSIs
- [ ] Lambda environment variable `DEVICE_REGISTRY_TABLE` set correctly
- [ ] Lambda has IAM permissions to read/write the registry table
- [ ] Physical verification: trigger device uplink, confirm registry entry in DynamoDB
- [ ] Verify SC- short ID matches expected SHA-256 for known device `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`

## Testing Requirements
- [ ] `terraform plan` shows only expected additions
- [ ] `aws dynamodb scan --table-name sidecharge-device-registry` returns auto-provisioned device
- [ ] CloudWatch logs show "Auto-provisioned device SC-XXXXXXXX" message

## Deliverables
- Deployed DynamoDB table in AWS
- Verified registry entry for physical device
