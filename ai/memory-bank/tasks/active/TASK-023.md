# TASK-023: BUG — PSA crypto AEAD error -149 after platform re-flash

**Status**: REOPENED (2026-02-17, Eero)
**Priority**: P1
**Owner**: —
**Branch**: —
**Size**: M (estimate — root cause investigation needed)

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

### Additional Context (TASK-047 testing session)

**Flash sequence used**: MFG (`rak-sid/mfg.hex`) → platform (`build/merged.hex`) → app (`build_app/app.hex`), all via pyocd. MFG was found at repo root — `flash.sh` uses `../../mfg.hex` which only resolves correctly when CWD is `app/rak4631_evse_monitor/`.

**Downlink sources in this system**:
- `decode_evse_lambda` → sends TIME_SYNC (0x30) as a direct response to each EVSE uplink. These **always decrypt OK** after correct flash order.
- `charge_scheduler_lambda` → runs on EventBridge schedule (every 5 min), sends charge control (0x10) or delay window (0x20) commands asynchronously. These are the ones that **intermittently fail** with -149.
- Both Lambdas use `sidewalk_utils.send_sidewalk_msg()` which calls `aws iotwireless send-data-to-wireless-device`. The Sidewalk cloud handles encryption — the Lambdas send plaintext payloads.

**Timing pattern**: Failing messages arrive ~15s after an uplink (e.g., 00:04:19, 00:07:19). Successful TIME_SYNC responses arrive ~3-5s after uplink. The 15s gap suggests the charge scheduler fires on its own cadence, not in response to uplinks.

**Key observation**: The PSA -149 error is in `sid_pal_crypto_aead_crypt`, which is the Sidewalk SDK's authenticated encryption function. The error means the decrypted message failed authentication. This could mean:
- The Sidewalk cloud is using a session key that the device doesn't have (session mismatch after reflash)
- The charge scheduler downlinks are queued/buffered by the Sidewalk cloud and encrypted with a stale session key from before the reflash
- The nonce/counter state is out of sync between the cloud and device for asynchronous messages

**MFG file location**: `/Users/emilyf/sidewalk-projects/rak-sid/mfg.hex` (also appears in each worktree via git).

### Hypotheses
1. **Stale queued downlinks**: The charge scheduler may have queued downlinks before the reflash, and these are being delivered with old session encryption. They would clear after the queue drains.
2. **Session key rotation**: Immediate responses use the current session key; delayed messages may use a stale key or different crypto context
3. **Multiple downlink sources**: charge_scheduler_lambda sends on its own EventBridge schedule — may use a different Sidewalk session than decode_evse_lambda responses
4. **BLE vs LoRa key mismatch**: First boot registers BLE, crypto may differ per link type
5. **Sidewalk SDK session state**: The SDK may maintain separate crypto state per message type or destination

### Investigation Plan
1. Check if PSA -149 errors persist beyond the first 10 minutes after reflash (queue drain theory)
2. Temporarily disable charge_scheduler to isolate whether its downlinks are the failing ones
3. Add logging in `sidewalk_dispatch.c` to print the raw encrypted bytes of failing messages
4. Check AWS IoT Wireless console for session/registration state after reflash

### Acceptance Criteria
- [ ] Root cause identified for intermittent -149 on correctly-flashed device
- [ ] All downlinks decrypt without error after MFG → platform → app flash
- [ ] KI-002 updated with complete resolution

## Prior Deliverables (original close)
- Updated `flash.sh` (platform subcommand warning)
- Updated `docs/known-issues.md` (KI-002 resolution)
