# Known Issues

Active known issues for the RAK Sidewalk EVSE Monitor. Each entry documents the symptom, root cause, impact, and workaround.

---

## KI-001: Version mismatch leaves device in platform-only mode

**Since**: ADR-001 (2026-02-11)
**Severity**: Medium — device is safe but non-functional for app tasks

### Symptom

After flashing a new platform or app image, `sid status` shows:
```
App image: NOT LOADED (version mismatch)
```

The device boots, Sidewalk connects, OTA engine works, but no EVSE telemetry is sent and no charge control commands are processed.

### Root Cause

The platform API version (`APP_CALLBACK_VERSION` in `platform_api.h`) doesn't match the app's compiled version. Per ADR-001, this is now a hard stop — the platform refuses to invoke mismatched app callbacks.

This happens when:
- Platform is reflashed with a new API version but the app isn't updated
- App is OTA'd with a new API version but the platform isn't reflashed
- Development builds where one side has the version bumped and the other doesn't

### Impact

- No EVSE telemetry uplinks
- No charge control (relay stays in last state)
- No shell `app` commands
- OTA engine **still works** — a corrected app can be pushed over-the-air

### Workaround

1. Check versions: `sid status` shows expected vs actual version
2. Flash matching images:
   - If platform was updated: reflash app with matching version via `flash.sh app`
   - If app was OTA'd: OTA the correct app version, or reflash platform + app via `flash.sh all`
3. If OTA is available: push the correct app version via `ota_deploy.py send`

### Resolution

Keep platform and app API versions in sync. Only bump `APP_CALLBACK_VERSION` when the function pointer table layout changes (add/remove/reorder). Most development changes don't require a version bump.

---

## KI-002: PSA crypto AEAD error -149 after platform re-flash

**Since**: 2026-02-11
**Severity**: High — device cannot send or receive encrypted Sidewalk messages
**Tracking**: TASK-023
**Status**: Resolved

### Symptom

Serial console shows:
```
[00:05:30.229,309] <err> sid_crypto: PSA Error code: -149 in sid_pal_crypto_aead_crypt
```

**Two distinct scenarios produce this error:**

1. **All crypto fails (HUK mismatch)**: Every downlink fails after platform reflash without MFG. This is the real bug — see Root Cause below.
2. **Intermittent BLE background noise**: Occasional -149 during normal operation from the Sidewalk SDK's BLE discovery/beacon processing. This is benign and does **not** affect LoRa message delivery. Documented in `provisioning.md` troubleshooting table.

### Root Cause

PSA error -149 is `PSA_ERROR_INVALID_SIGNATURE`. The Hardware Unique Key (HUK) used by PSA crypto is derived from the nRF52840's key storage. Reflashing the platform without first reflashing MFG credentials invalidates the HUK, causing PSA key derivation to produce keys that don't match the stored session keys.

The health check (`mfg_key_health_check()`) detects **missing** keys but not **mismatched** HUK — so it passes even when crypto is broken.

### Impact

When HUK is mismatched (scenario 1):
- All Sidewalk messages fail encryption/decryption
- Device is effectively offline (connected but unable to communicate)
- OTA also fails if OTA messages require encryption

BLE background noise (scenario 2):
- No impact on LoRa application messages
- No action required

### Resolution

**HUK mismatch prevention:**
- `flash.sh platform` warns about HUK invalidation and requires confirmation
- `flash.sh all` uses correct order (MFG → platform → app)
- Correct flash order: `flash.sh mfg` → `flash.sh platform` → `flash.sh app`

**Charge scheduler delivery reliability** (2026-02-17):
- Changed `charge_scheduler_lambda` from `transmit_mode=0` (best-effort) to `transmit_mode=1` (reliable). Best-effort downlinks could be silently dropped when no LoRa Class A RX window was open, giving the appearance of failed delivery. This was incorrectly attributed to PSA -149 errors.

Remaining gap: no runtime detection of HUK mismatch at boot. Would require a test decryption of a stored session key during `sidewalk_event_platform_init()`.

---

## KI-003: Stale flash data inflates OTA delta baselines

**Since**: 2026-02-11
**Severity**: Medium — delta OTA works but sends more chunks than necessary
**Tracking**: TASK-022
**Status**: Resolved

### Symptom

`ota_deploy.py baseline` reports a baseline significantly larger than the actual app binary. For example, baseline shows 4524 bytes when the app binary is 239 bytes.

### Root Cause

pyOCD only erases flash pages it writes to. When flashing a smaller app over a larger one, pages beyond the new image retain old code. `ota_deploy.py baseline` reads the full 256KB app partition and trims trailing 0xFF — but stale non-0xFF bytes from the old image survive in the middle.

Same issue after OTA apply: the apply loop only copies pages for the new image size, leaving stale pages from a previous larger image.

### Impact

- Delta OTA computes against an inflated baseline
- More chunks are marked as "changed" than actually changed
- OTA transfer takes longer than necessary over LoRa (each chunk is ~19 bytes, ~3s per chunk)
- Not a correctness issue — the resulting firmware is still correct

### Workaround

Before capturing a baseline, reflash the app to ensure clean flash:
```bash
./flash.sh app   # after fix: will erase partition first
```

### Resolution

**Status**: Resolved (TASK-022, 2026-02-17)

Three-layer fix implemented:
1. `flash.sh`: Erases app partition (0x90000-0xCEFFF) before writing app hex
2. `ota_update.c`: Erases stale pages beyond new image after OTA apply (full, delta, and recovery paths)
3. `ota_deploy.py`: Warns if baseline dump is significantly larger than app binary size
