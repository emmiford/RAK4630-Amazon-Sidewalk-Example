# Experiment Log

Structured experiments to resolve open technical questions. Each experiment follows: hypothesis, method, results, verdict.

See also: [ADR index](adr/README.md) for decisions, [Known Issues](known-issues.md) for active bugs.

---

## EXP-001: Can Sidewalk operate LoRa-only without BLE compiled in?

**Filed**: 2026-02-25 (Oliver)
**Status**: PROPOSED
**Triggered by**: ADR-010 risk — "not fully confirmed whether the Sidewalk SDK requires BLE hardware or merely expects the BLE stack to compile and link"
**Blocks**: Future module selection decisions (e.g., LoRa-E5 Mini, RAK3172, any BLE-less module)
**Task**: TASK-115

### Background

The project uses the RAK4630 (nRF52840 + SX1262), which has both BLE and LoRa hardware. In production, BLE is only used for initial Sidewalk registration (~15 seconds, once per device lifetime). After that, the device operates LoRa-only forever.

The open question: could we use a **cheaper LoRa-only module** (no BLE hardware at all) for production devices, if we do initial registration on a different board that has BLE, then move the session keys to the LoRa-only board?

This matters because:
- RAK4630 costs ~$15; LoRa-only modules cost ~$5-8
- At scale (thousands of units), the BLE radio we never use adds $7-10K+ of unnecessary BOM cost
- Simpler modules = smaller footprint, lower power, fewer failure modes

But there are **three layers** where BLE dependency might lurk:

| Layer | Question |
|-------|----------|
| **Build-time** | Does the Sidewalk SDK compile/link without `CONFIG_BT`? |
| **Init-time** | Does `sid_platform_init()` / `sid_init()` fail if no BLE stack is present? |
| **Runtime** | Does LoRa uplink/downlink work if BLE was never initialized? |

### Hypotheses

**H1 (optimistic)**: The Sidewalk SDK's BLE dependency is purely a link-mask selection. If you set `link_mask = SID_LINK_TYPE_3` (LoRa only) and the session keys are already provisioned, the SDK will work without BLE compiled in.

**H2 (pessimistic)**: The SDK has hard compile-time or init-time dependencies on the BLE stack (GATT tables, crypto session setup, internal state machines) that cause build failures or runtime crashes even when BLE is never used over the air.

**H0 (null — current assumption)**: We don't know, so we conservatively require BLE hardware on every board (ADR-010 status quo).

### Method

Four phases, each building on the previous. **Stop as soon as a phase fails** — that's the answer.

#### Phase 1: Compile-time test (no hardware needed)

**Goal**: Determine if the firmware builds without BLE Kconfig options.

1. Create a test overlay `no_ble.conf` that disables all BLE:
   ```
   CONFIG_BT=n
   CONFIG_SIDEWALK_LINK_MASK_BLE=n
   CONFIG_SIDEWALK_LINK_MASK_LORA=y
   ```
2. Remove `CONFIG_BT_DEVICE_NAME` from `prj.conf` (or override to empty — Kconfig may reject it if BT is off)
3. Build the platform firmware with the additional overlay:
   ```
   nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
     "cd /Users/emilyf/sidewalk-projects && west build -p -b rak4631 \
      rak-sid/app/rak4631_evse_monitor/ -- \
      -DOVERLAY_CONFIG='lora.conf;no_ble.conf'"
   ```
4. Record: does it compile? Does it link? What errors?

**Pass criteria**: Clean build with no BLE-related link errors.
**Fail criteria**: Unresolved symbols, missing BLE GATT tables, Kconfig dependency errors.

**If Phase 1 fails**: Catalog every BLE symbol/header the SDK requires. This tells us whether stubbing is feasible or if the dependency is too deep to work around.

#### Phase 2: Boot test (needs RAK4631, no gateway needed)

**Goal**: Determine if the firmware boots and Sidewalk initializes without BLE.

Prerequisites: Phase 1 passes.

1. Flash the no-BLE build onto a RAK4631 that has **already been registered** (session keys present at 0xF8000)
2. Connect serial console (`screen /dev/cu.usbmodem101 115200`), observe boot log
3. Run `sid status` — check init state
4. Check for:
   - `SID_INIT_STARTED_OK` vs error states
   - Any BLE-related crash or fault
   - LoRa radio init success (`radio init err` absent)

**Pass criteria**: `sid status` shows `STARTED_OK`, no crash, radio initialized.
**Fail criteria**: `sid_init()` or `sid_start()` returns error, or device crashes during init.

**Recovery**: If boot fails, reflash the normal BLE-enabled build. Session keys survive platform reflash as long as MFG is intact.

#### Phase 3: LoRa connectivity test (needs Sidewalk gateway in range)

**Goal**: Confirm LoRa uplinks and downlinks work with the no-BLE build.

Prerequisites: Phase 2 passes.

1. With the no-BLE firmware running, trigger an uplink: `sid send` or `app sid send`
2. Check DynamoDB for received uplink data
3. Send a downlink via AWS CLI or `charge_scheduler_lambda` test event
4. Check serial console for downlink receipt
5. Let the device run for 10+ minutes to confirm periodic heartbeats arrive

**Pass criteria**: Uplinks appear in DynamoDB, downlink received on device, heartbeats stable.
**Fail criteria**: Uplinks not delivered, downlink not received, or connectivity degrades over time.

#### Phase 4: Session key portability test (needs two boards)

**Goal**: Confirm session keys from a BLE-registered board work on a different physical board.

Prerequisites: Phase 3 passes. This is the "production scenario" — register on a BLE-capable jig, deploy on a LoRa-only production board.

1. Take a **registered** RAK4631 (Board #1) — back up its session keys:
   ```
   pyocd cmd -c "savemem 0xF8000 0x7000 board1_keys.bin" --target nrf52840
   ```
2. Take a **second** RAK4631 (Board #2) — flash the same MFG page as Board #1, plus the no-BLE platform build, plus app
3. Restore Board #1's session keys onto Board #2:
   ```
   pyocd flash --target nrf52840 --base-address 0xF8000 board1_keys.bin
   ```
4. Boot Board #2, check `sid status`, send uplinks, receive downlinks

**Pass criteria**: Board #2 operates identically to Board #1 on LoRa, using Board #1's session keys.
**Fail criteria**: AWS rejects the device (device ID mismatch, key binding to hardware), or Sidewalk SDK rejects the transferred keys.

**Important caveat**: Phase 4 uses two RAK4631 boards (both have BLE hardware physically present). This proves the *concept* of key portability and no-BLE firmware, but doesn't prove a true BLE-less SoC (e.g., STM32WLE5) would work. A Phase 5 with actual BLE-less hardware would be needed for full validation — but Phase 4 removes the biggest unknowns cheaply.

### Success criteria (overall)

| Result | Meaning | Next step |
|--------|---------|-----------|
| Phase 1 fails | BLE is a hard compile-time dependency | Catalog missing symbols; evaluate stub feasibility or accept BLE hardware requirement |
| Phase 2 fails | SDK init requires BLE internally | BLE hardware required on every board |
| Phase 3 fails | LoRa transport depends on BLE state | Unexpected — would indicate SDK bug or undocumented coupling |
| Phase 4 fails | Session keys are hardware-bound | Registration must happen on the production board — no key portability, no registration jig concept |
| All pass | LoRa-only module is viable for production | Revisit ADR-010, evaluate RAK3172 or LoRa-E5 Mini, design registration jig |

### Effort estimate

| Phase | Time | Hardware |
|-------|------|----------|
| Phase 1 — Compile | ~30 min | None |
| Phase 2 — Boot | ~30 min | 1x RAK4631 + programmer |
| Phase 3 — Connectivity | ~30 min | + Sidewalk gateway |
| Phase 4 — Key portability | ~1 hr | + 2nd RAK4631 |
| **Total** | **~2.5 hrs** | |

### Equipment needed

- RAK4631 Board #1 (already registered, session keys present)
- RAK4631 Board #2 (for Phase 4 — Board #2 with CC310 defect is fine since we're testing LoRa not PSA crypto)
- NanoDAP programmer + pyOCD
- Amazon Echo / Sidewalk gateway in range (Phase 3)
- Serial console access (USB CDC)

### Risks and mitigations

| Risk | Mitigation |
|------|------------|
| No-BLE build bricks the board | Reflash normal build; session keys survive platform reflash |
| Session key backup is corrupt | Verify backup with hex dump before overwriting anything |
| Board #2 CC310 defect interferes with results | CC310 issue is in PSA crypto init, not LoRa transport; skip Board #2 or use a clean board if it confounds results |
| False positive: works on nRF52840 but not on true BLE-less silicon | Acknowledged — Phase 4 note above. Would need Phase 5 with actual BLE-less hardware for full validation |
| Sidewalk SDK version change breaks assumptions | Pin to NCS v2.9.1 for the experiment; document SDK version in results |

### Results

*Not yet started.*

### Verdict

*Pending experiment execution.*
