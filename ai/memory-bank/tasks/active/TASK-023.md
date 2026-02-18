# TASK-023: BUG — PSA crypto AEAD error -149 after platform re-flash

**Status**: done (2026-02-17, Eliel — verified: Eero)
**Priority**: P1
**Owner**: Eliel
**Branch**: `task/023-psa-crypto-149`
**Size**: M

## Original Summary (2026-02-11)
Root cause confirmed: HUK regenerated on platform flash, MFG keys still present, so `mfg_key_health_check()` passes but PSA derives wrong keys. `flash.sh platform` now warns about HUK invalidation and requires confirmation. KI-002 updated.

## Reopened — New Findings (2026-02-17)

**The original fix is insufficient.** Even after correct MFG → platform → app flash order, PSA -149 errors persist intermittently on downlink decryption.

### Observed behavior (TASK-047 verification session)
- **Without MFG reflash**: ALL downlinks fail with PSA -149
- **With MFG → platform → app flash**: MOST downlinks succeed, but some still fail
- **Pattern**: Immediate downlink responses (~2-5s after uplink) decrypt OK. Delayed/asynchronous downlinks (~15s after uplink) fail with -149.

### Evidence (serial log 2026-02-17)
```
[00:04:04] Received message: 30 42 6e 3f ...  ← TIME_SYNC, decrypted OK
[00:04:19] PSA Error code: -149               ← Async downlink, FAIL
[00:05:04] Received message:                  ← Response to uplink, OK
[00:06:04] Received message:                  ← Response to uplink, OK
[00:07:19] PSA Error code: -149               ← Async downlink, FAIL
```

## Investigation & Root Cause (Eliel, 2026-02-17)

### Two distinct -149 scenarios

The reopened report conflated **two separate issues**:

1. **HUK mismatch (real bug, resolved in original close)**: Platform reflash without MFG invalidates HUK → ALL crypto fails. Fix: correct flash order (MFG → platform → app). This works correctly.

2. **BLE background noise (not a bug)**: Intermittent -149 errors from the Sidewalk SDK's internal BLE discovery/beacon processing. These occur during normal operation and have **no impact on LoRa message delivery**.

### Evidence that intermittent -149 is BLE noise

- `provisioning.md` (line 246): `PSA -149 INVALID_SIGNATURE` documented as "Normal Sidewalk background noise — Ignore, not an error"
- `RESULTS-2026-02-11.md` (line 40): "BLE-related, does not affect LoRa operation. Pre-existing."
- Device uses BLE + LoRa (`SID_LINK_TYPE_1 | SID_LINK_TYPE_3`). BLE registration generates periodic -149 as part of protocol operations.
- The ~3 minute cadence of -149 errors doesn't match charge_scheduler's 5-minute EventBridge schedule.
- The `on_sidewalk_msg_received()` callback is only invoked for **successfully decrypted** messages. If a LoRa downlink failed AEAD, the callback wouldn't fire and no `Received message:` log would appear. The interleaved pattern of successful receives + -149 errors confirms different sources (LoRa app messages vs BLE protocol noise).

### Separate delivery issue found and fixed

The charge_scheduler used `transmit_mode=0` (best-effort), meaning downlinks could be **silently dropped** when no LoRa Class A RX window was open. This gave the appearance of "failing" messages when they were actually never delivered. Changed to `transmit_mode=1` (reliable) to ensure charge control commands are retried until acknowledged.

### Acceptance Criteria
- [x] Root cause identified: intermittent -149 is BLE background noise, not app crypto failure
- [x] All LoRa downlinks decrypt without error after MFG → platform → app flash (was already true — BLE noise was a red herring)
- [x] KI-002 updated with complete resolution distinguishing HUK mismatch from BLE noise

## Deliverables

### Original close (2026-02-11)
- Updated `flash.sh` (platform subcommand warning)
- Updated `docs/known-issues.md` (KI-002 resolution)

### Reopened resolution (2026-02-17)
- `charge_scheduler_lambda.py`: `transmit_mode=0` → `transmit_mode=1` for both `send_charge_command()` and `send_delay_window()` — reliable delivery for charge control
- `aws/tests/test_charge_scheduler.py`: Updated transmit_mode assertions (0 → 1)
- `docs/known-issues.md` (KI-002): Distinguished HUK mismatch (real bug) from BLE background noise (benign); documented charge_scheduler transmit_mode fix
