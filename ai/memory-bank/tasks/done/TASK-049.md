# TASK-049: Deploy device registry — terraform apply + verify uplink creates entry

**Status**: merged done (2026-02-17, Eliel)
**Priority**: P0
**Owner**: Eliel
**Branch**: task/049-device-registry-deploy (merged to main)
**Size**: XS (1 point)

## Description
TASK-036 merged code for the device registry DynamoDB table, but `terraform apply` had not been run yet. This task deployed the infrastructure, fixed two bugs discovered during physical verification, and confirmed auto-provisioning works on real device uplinks.

## Dependencies
**Blocked by**: TASK-036 (DONE)
**Blocks**: TASK-037 (utility identification needs registry deployed)

## What Was Done
1. `terraform apply` — created `sidecharge-device-registry` table + updated 3 Lambdas + IAM policy
2. **Bugfix #1**: Lambda only called `update_last_seen()`, never `get_or_create_device()`. Added the call so first uplink fully provisions the device record.
3. **Bugfix #2**: `get_or_create_device()` set `owner_email=""` which caused `ValidationException` — DynamoDB rejects empty strings as GSI key attributes. Fixed by omitting `owner_email` and `location` from initial record (sparse GSI correctly excludes unowned devices).
4. **IAM fix**: Added `dynamodb:Scan` on registry table + `iotwireless:SendDataToWirelessDevice` + `iotwireless:ListWirelessDevices` to decode Lambda IAM role. Required for device lookup (TIME_SYNC downlinks) and fallback discovery.
5. Physical verification: device SC-C014EA63 auto-provisioned on uplink at 2026-02-17T23:26:59Z.

## Acceptance Criteria
- [x] `terraform apply` succeeds — `sidecharge-device-registry` table created with both GSIs
- [x] Lambda environment variable `DEVICE_REGISTRY_TABLE` set correctly
- [x] Lambda has IAM permissions to read/write the registry table
- [x] Physical verification: device uplink auto-provisions registry entry in DynamoDB
- [x] SC-C014EA63 matches expected SHA-256 for device `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`

## Testing Requirements
- [x] `terraform plan` shows only expected additions
- [x] `aws dynamodb scan --table-name sidecharge-device-registry` returns auto-provisioned device
- [x] CloudWatch logs show "Auto-provisioned device SC-C014EA63" message
- [x] 185 Python tests pass (including updated device_registry tests)

## Verified DynamoDB Record
```json
{
  "device_id": "SC-C014EA63",
  "wireless_device_id": "b319d001-6b08-4d88-b4ca-4d2d98a6d43c",
  "sidewalk_id": "BFFF8E87CE",
  "status": "active",
  "app_version": 0,
  "last_seen": "2026-02-17T23:28:58Z",
  "created_at": "2026-02-17T23:26:59Z"
}
```
