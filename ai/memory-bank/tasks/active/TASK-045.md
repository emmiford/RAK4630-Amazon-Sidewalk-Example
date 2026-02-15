# TASK-045: Integrate real ED25519 verify library into platform firmware

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: `task/045-ed25519-verify-lib`
**Size**: M (3 points)

## Description
TASK-031 added OTA image signing with a placeholder `ota_signing.c` that always returns success. This task replaces the placeholder with a real ED25519 verify-only implementation. Options: (1) PSA Crypto API if Mbed TLS in NCS v2.9.1 supports ED25519 (`PSA_ALG_PURE_EDDSA`), or (2) standalone ed25519-donna (~3-4KB verify-only). The 32-byte public key constant must be compiled into firmware. Requires platform rebuild and reflash.

## Dependencies
**Blocked by**: TASK-031 (signing infrastructure — DONE)
**Blocks**: TASK-046 (E2E signed OTA verification needs real verify)

## Acceptance Criteria
- [ ] `ota_signing.c` calls a real ED25519 verify function (PSA Crypto or ed25519-donna)
- [ ] 32-byte public key embedded as a constant (generated via `ota_deploy.py keygen`)
- [ ] Platform firmware builds with ED25519 verify (~3-4KB code size increase acceptable)
- [ ] Host-side C tests still pass (mock_ota_signing unaffected)
- [ ] Unsigned images pass through (no signature check when `is_signed=false`)

## Testing Requirements
- [ ] Platform build succeeds with real ED25519 library
- [ ] On-device: `ota_verify_signature()` returns 0 for valid, nonzero for invalid
- [ ] Code size delta documented

## Deliverables
- `app/rak4631_evse_monitor/src/ota_signing.c` (real verify)
- Kconfig or CMake changes for ED25519 library inclusion
- Documentation: which library chosen and why
