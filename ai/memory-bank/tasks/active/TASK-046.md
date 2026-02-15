# TASK-046: E2E signed OTA verification on physical device

**Status**: not started
**Priority**: P1
**Owner**: Eero
**Branch**: `task/046-signed-ota-e2e`
**Size**: S (2 points)

## Description
End-to-end verification that the full signing pipeline works on hardware: keygen, sign, deploy, device receives signed image, verifies ED25519 signature, and applies. Also verify negative case: tamper with S3 binary and confirm device rejects with SIG_ERR. Final validation gate for TASK-031 signing infrastructure.

## Dependencies
**Blocked by**: TASK-045 (real ED25519 verify library must be integrated first)
**Blocks**: none

## Acceptance Criteria
- [ ] Generate keypair via `ota_deploy.py keygen`
- [ ] Deploy signed firmware via `ota_deploy.py deploy --build --version N`
- [ ] Device receives all chunks, CRC32 passes, ED25519 signature verifies, image applied
- [ ] Device reboots and runs new firmware successfully
- [ ] Negative test: tamper with S3 binary → device rejects with OTA_STATUS_SIG_ERR (5)
- [ ] Negative test: deploy with `--unsigned` → device accepts (backward compatible)
- [ ] Results documented in E2E results file

## Testing Requirements
- [ ] Requires physical device with platform firmware containing real ED25519 verify
- [ ] Requires AWS infrastructure (S3, Lambda, IoT Wireless)
- [ ] Serial monitor for OTA state machine transitions

## Deliverables
- `tests/e2e/RESULTS-signed-ota.md`
- Updated `tests/e2e/RUNBOOK.md`
