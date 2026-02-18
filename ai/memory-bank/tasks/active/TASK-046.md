# TASK-046: E2E signed OTA verification on physical device

**Status**: coded (2026-02-18, Eero) — E2E PASS, pending merge
**Priority**: P1
**Owner**: Eero
**Branch**: `task/046-signed-ota-e2e`
**Size**: S (2 points)

## Description
End-to-end verification that the full signing pipeline works on hardware: keygen, sign, deploy, device receives signed image, verifies ED25519 signature, and applies. Also verify negative case: tamper with S3 binary and confirm device rejects with SIG_ERR. Final validation gate for TASK-031 signing infrastructure.

## Dependencies
**Blocked by**: TASK-023 (resolved — PSA -149 is KI-002, not blocking OTA)
**Blocks**: none

## Acceptance Criteria
- [x] Generate keypair via `ota_deploy.py keygen`
- [x] Deploy signed firmware via `ota_deploy.py deploy --build --version N`
- [x] Device receives all chunks, CRC32 passes, ED25519 signature verifies, image applied
- [x] Device reboots and runs new firmware successfully
  - Confirmed: uplink payload byte 1 = 0x0c (v12) in CloudWatch decode logs
- [ ] Negative test: tamper with S3 binary → device rejects with OTA_STATUS_SIG_ERR (5)
- [ ] Negative test: deploy with `--unsigned` → device accepts (backward compatible)
- [x] Results documented in E2E results file

## Issues Found & Fixed (this session)
1. **Lambda sending to wrong device**: `ota-sender-lambda-role` lacked DynamoDB permissions for
   `sidecharge-device-registry` table. Fell back to `list_wireless_devices` → wrong device ID
   (`6b3b5b8e` instead of `b319d001`). Fixed in terraform: added `dynamodb:GetItem`+`Scan` +
   `DEVICE_REGISTRY_TABLE` env var for both ota-sender and charge-scheduler Lambdas.
2. **ED25519 verify buffer overflow**: `ota_page_buf` was 4KB (`OTA_FLASH_PAGE_SIZE`), but
   firmware is ~11.4KB. ED25519 requires full message in RAM. Added `OTA_VERIFY_BUF_SIZE=16384`
   (16KB), updated size checks in both full and delta verify paths. RAM: 47% used (plenty of headroom).
3. **Flash script wrong binary**: `flash.sh` relative path `../../build/merged.hex` resolved to
   stale `rak-sid/build/` (Feb 8) instead of `sidewalk-projects/build/` (today). Used pyocd directly.
4. **Hardcoded log string**: `"EVSE TX v08"` was hardcoded in `app_tx.c` format string, not derived
   from `EVSE_VERSION`. Fixed to use `"v%02x", EVSE_VERSION`.

## Full E2E Results (2026-02-18)
1. **OTA_START delivery**: 19-byte signed OTA_START received by device (PASS)
2. **Delta chunks**: 6/6 chunks sent and ACKed, all status=0 (PASS)
3. **CRC32 validation**: Device computed CRC over merged delta image, matched expected (PASS)
4. **ED25519 signature**: Verified OK with 16KB buffer (PASS — after fix)
5. **Delta apply**: Staging + primary merged → primary, magic check passed (PASS)
6. **Reboot**: Device rebooted with new firmware (PASS)
7. **Cloud verify**: Decoded uplink shows `e5 0c ...` — version byte = 0x0c (PASS)

## Code Changes (uncommitted on main)
- `aws/terraform/main.tf`: IAM permissions + env vars for device registry
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c`: Fix hardcoded version log + revert EVSE_VERSION
- `app/rak4631_evse_monitor/include/ota_update.h`: `OTA_VERIFY_BUF_SIZE 16384` (on task-046 branch)
- `app/rak4631_evse_monitor/src/ota_update.c`: Enlarged verify buffer + updated checks (on task-046 branch)

## Deliverables
- [x] Partial results: `tests/e2e/RESULTS-task046-signed-ota.md` on branch
- [x] Full E2E results documented above
- [ ] Negative tests (tamper + unsigned) — deferred to follow-up
