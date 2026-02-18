# TASK-045: Integrate real ED25519 verify library into platform firmware

**Status**: committed (2026-02-17, Eliel)
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
- [x] `ota_signing.c` calls a real ED25519 verify function (PSA Crypto or ed25519-donna)
- [x] 32-byte public key embedded as a constant (generated via `ota_deploy.py keygen`)
- [x] Platform firmware builds with ED25519 verify (~3-4KB code size increase acceptable)
- [x] Host-side C tests still pass (mock_ota_signing unaffected)
- [x] Unsigned images pass through (no signature check when `is_signed=false`)

## Testing Requirements
- [x] Platform build succeeds with real ED25519 library
- [ ] On-device: `ota_verify_signature()` returns 0 for valid, nonzero for invalid
- [x] Code size delta documented

## Implementation Notes

**Library chosen**: PSA Crypto API (`PSA_ALG_PURE_EDDSA`) via NCS nrf_security Oberon backend.

**Why PSA Crypto over ed25519-donna**:
- PSA Crypto / Oberon already linked into firmware (Sidewalk uses it)
- Ed25519 support (`PSA_WANT_ALG_PURE_EDDSA`, `PSA_WANT_ECC_TWISTED_EDWARDS_255`) already available in NCS v2.9.1 — the Oberon backend provides Ed25519 since CC310 does not
- No new source files needed — just Kconfig options + PSA API calls
- Standard API, consistent with rest of crypto stack

**Code size delta**: +428 bytes text, +4 bytes data = **+432 bytes total** (well under 3-4KB budget). The Oberon Ed25519 implementation was already linked as part of the standard nrf_security configuration.

**Signing keypair**: Generated via `python3 aws/ota_deploy.py keygen`. Keys stored at `~/.sidecharge/ota_signing.key` (private) and `~/.sidecharge/ota_signing.pub` (public). The raw 32-byte public key is embedded in `ota_signing.c`.

## Deliverables
- `app/rak4631_evse_monitor/src/ota_signing.c` (real verify via PSA Crypto)
- `app/rak4631_evse_monitor/prj.conf` (Ed25519 Kconfig options)
- Documentation: this file
