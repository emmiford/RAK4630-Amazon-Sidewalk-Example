# TASK-049: Deploy device registry — terraform apply + verify uplink creates entry

**Status**: coded (2026-02-14, Eliel)
**Priority**: P0
**Owner**: Eliel
**Branch**: main (infrastructure deployment + Lambda bugfix)
**Size**: XS (1 point)

## Description
TASK-036 merged code for the device registry DynamoDB table, but `terraform apply` has not been run yet. The table `sidecharge-device-registry` does not exist in AWS. This task deploys the infrastructure and verifies the next device uplink auto-provisions a registry entry.

## Dependencies
**Blocked by**: TASK-036 (DONE)
**Blocks**: TASK-037 (utility identification needs registry deployed)

## What Was Done
1. `terraform apply` — created `sidecharge-device-registry` table + updated 3 Lambdas + IAM policy
2. **Bugfix**: Lambda only called `update_last_seen()` (bare upsert), never `get_or_create_device()`. Added the call so first uplink fully provisions the device record with all fields + logs "Auto-provisioned device SC-XXXXXXXX".
3. Redeployed Lambda with fix.
4. Computed expected short ID: **SC-C014EA63** for device `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`.

## Acceptance Criteria
- [x] `terraform apply` succeeds — `sidecharge-device-registry` table created with both GSIs
- [x] Lambda environment variable `DEVICE_REGISTRY_TABLE` set correctly
- [x] Lambda has IAM permissions to read/write the registry table
- [ ] Physical verification: trigger device uplink, confirm registry entry in DynamoDB
- [x] Verify SC- short ID matches expected SHA-256 for known device `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`

## Testing Requirements
- [x] `terraform plan` shows only expected additions
- [ ] `aws dynamodb scan --table-name sidecharge-device-registry` returns auto-provisioned device
- [ ] CloudWatch logs show "Auto-provisioned device SC-XXXXXXXX" message

## Deliverables
- Deployed DynamoDB table in AWS
- Verified registry entry for physical device (pending — device offline since Feb 12)
