# TASK-046: Signed OTA E2E Verification

**Date**: 2026-02-17 (in progress)
**Status**: BLOCKED by KI-002 (PSA crypto -149)
**Firmware**: main @ v0x0a (version 10), signed with ED25519
**Tester**: Eero
**Device**: RAK4631 on RAK5005-O baseboard, Sidewalk LoRa link

## Summary

OTA signing pipeline validated end-to-end from cloud through Lambda to S3.
Device-side verification blocked by KI-002 (PSA crypto error -149) which
intermittently drops downlink payloads, preventing the OTA_START message
from reaching the app layer.

## Cloud-Side Verification (PASS)

| Step | Result | Details |
|------|--------|---------|
| Keypair exists | PASS | `~/.sidecharge/ota_signing.key` + `.pub` |
| Embedded key matches | PASS | `ota_signing.c` matches generated key |
| Build + version patch | PASS | v0x0a, 11,420 bytes, CRC32=0x862a8485 |
| ED25519 sign | PASS | 11,420B + 64B sig = 11,484B |
| S3 upload (signed) | PASS | `firmware/app-v10.bin` with metadata `signed=true` |
| Lambda S3 trigger | PASS | Detected signed firmware, computed delta |
| Lambda baseline (fresh) | PASS | 11,420B, CRC32=0x37b340ad (after cold-start fix) |
| Lambda delta computation | PASS | 6/766 chunks changed [209, 761-765] |
| OTA_START sent | PASS | `size=11484 chunks=6 crc=0x12dccbfb ver=10 flags=0x01(signed)` |
| Retry timer | PASS | Re-sends every 60s on stale session |

## Device-Side Verification (BLOCKED)

| Step | Result | Details |
|------|--------|---------|
| OTA_START received | BLOCKED | PSA -149 dropping 19-byte downlinks |
| Chunk reception | BLOCKED | Requires OTA_START first |
| CRC32 verification | BLOCKED | — |
| ED25519 signature verify | BLOCKED | — |
| Image applied + reboot | BLOCKED | — |
| Negative: tampered binary → SIG_ERR | BLOCKED | — |
| Negative: --unsigned → accepts | BLOCKED | — |

## Issues Encountered

### KI-002: PSA crypto -149 (blocking)
The Sidewalk crypto layer intermittently fails to decrypt AEAD-encrypted
downlinks. Symptom: `sid_crypto: PSA Error code: -149` followed by
`Received message:` with no hex dump (payload lost). Some messages get
through (2-byte abort, 10-byte delay window), but the 19-byte OTA_START
has not successfully passed through in multiple attempts.

**Root cause**: PSA key derivation fails when HUK is invalidated by
platform reflash without MFG credentials re-flash. See KI-002 in
`docs/known-issues.md`.

**Fix**: TASK-023 (reopened) — fix PSA crypto, then retry TASK-046.

### Lambda baseline caching
The Lambda warm container caches S3 baseline downloads in memory. When
the device baseline changes (pyOCD reflash), the Lambda uses stale data
until cold-started. Fixed by updating a Lambda env var to force cold start.
Consider adding cache invalidation on S3 baseline upload (future improvement).

## Resume Instructions

After TASK-023 (PSA fix) is resolved:
1. Flash latest firmware: `bash rak-sid/app/rak4631_evse_monitor/flash.sh app`
2. Capture baseline: `python3 rak-sid/aws/ota_deploy.py baseline`
3. Deploy signed OTA: `python3 rak-sid/aws/ota_deploy.py deploy --build --version 11 --remote --force`
4. Monitor: `python3 rak-sid/aws/ota_deploy.py status --watch`
5. Verify on serial: OTA_START → chunks → CRC PASS → SIG PASS → reboot
6. Negative test: tamper S3 binary → verify SIG_ERR
7. Negative test: deploy with --unsigned → verify acceptance
