# TASK-031: OTA image signing — ED25519 signatures on OTA images

**Status**: MERGED DONE (2026-02-14, Eliel)
**Branch**: `task/031-ota-image-signing` (commit `07fc5f9`)

## Summary
ED25519 signing implemented end-to-end. 64-byte signature appended to app.bin before S3 upload. Flags byte in OTA_START (byte 19) signals signed firmware. Backward-compatible: old firmware ignores extra byte. Device verifies signature after CRC32. 13 new files, +1217 lines. 9 C tests + 16 Python tests all passing. Note: device-side uses placeholder verify (always success) — real ED25519 lib deferred to TASK-045.

## Deliverables
- `aws/ota_signing.py` (keygen, sign, verify)
- `aws/ota_deploy.py` (keygen subcmd, --unsigned flag)
- `aws/ota_sender_lambda.py` (flags byte via S3 metadata)
- `app/rak4631_evse_monitor/include/ota_signing.h`
- `app/rak4631_evse_monitor/src/ota_signing.c` (placeholder verify)
- `aws/tests/test_ota_signing.py` (16 tests)
- `tests/app/test_ota_signing.c` (9 tests)
- `tests/mocks/mock_ota_signing.c`
