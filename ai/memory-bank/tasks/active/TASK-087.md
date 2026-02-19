# TASK-087: Generate + provision production auth key (device + cloud)

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
TASK-032 implemented command authentication but left the device key as a commented-out placeholder in `app_entry.c`. This task generates a real 32-byte HMAC key, compiles it into the firmware, deploys it to the Lambda via Terraform, and OTA-updates the device — activating end-to-end auth.

Requires coordinating: firmware build → OTA deploy → Terraform apply. The Lambda should be updated first (or simultaneously) so signed commands reach the device before the device starts requiring them.

## Dependencies
**Blocked by**: TASK-086 (Terraform infra for the key)
**Blocks**: none

## Acceptance Criteria
- [ ] Production 32-byte key generated and stored securely
- [ ] Key compiled into `app_entry.c` (uncomment + fill real bytes)
- [ ] Firmware built and OTA-deployed with the key
- [ ] Lambda env var set to the same key (via TASK-086 Terraform)
- [ ] On-device verification: signed charge control command accepted
- [ ] On-device verification: unsigned command rejected (log shows "auth verification failed")
- [ ] Key documented in secure internal key inventory (not in git)

## Testing Requirements
- [ ] On-device E2E: trigger scheduler during TOU peak → device receives signed delay window → pauses charging
- [ ] On-device E2E: send unsigned 0x10 command via `aws iotwireless` CLI → device rejects with error log
- [ ] Rollback plan: if auth causes issues, OTA back to keyless firmware

## Deliverables
- Updated `app_entry.c` with real key
- OTA firmware image (signed, deployed)
- Terraform apply (key in Lambda env)
- E2E test log (serial capture showing auth OK + auth failed)
