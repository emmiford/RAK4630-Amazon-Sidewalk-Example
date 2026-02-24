# EXP-012: Board #1 AIN1 Pin Recovery + RAK19001 Connector Validation

**Status**: Concluded
**Verdict**: DOCUMENTED
**Type**: Hardware diagnostic
**Date**: 2026-02-22
**Owner**: Oliver
**Related**: EXP-009b, EXP-010, REC-007, REC-008, TASK-104, TASK-113

> **Board numbering correction (2026-02-22)**: Board #1 and Board #2 labels were swapped in all prior documentation. Board #1 is physically marked "1" on the module.

---

## Problem Statement

Board #1 has two overlapping pin problems:
1. **P0.31 (AIN1) reads 0mV permanently** — after a voltmeter was connected on Feb 20, the board reset and AIN1 was grounded thereafter. Could be SAADC analog mux latch (recoverable) or ESD damage (permanent).
2. **RAK19001 even-row connector seating failure** (EXP-010) — one row of the 40-pin Hirose DF40C connector has no electrical contact.

These must be separated and addressed independently.

## Hypothesis

**Hypothesis A (SAADC latch)**: The board reset interrupted an active SAADC sample, latching the analog mux in a state that grounds P0.31. Adding `nrf_saadc_disable(NRF_SAADC)` at boot will release the latch.

**Hypothesis B (ESD damage)**: The voltmeter probe injected an ESD pulse that permanently damaged P0.31's internal protection diode.

**Success Metrics**:
- Primary: determine which hypothesis is correct
- Secondary: if A, recover AIN1 via firmware; if B, confirm pin dead and plan alternate

## Hardware Inventory

| Item | Status | Notes |
|------|--------|-------|
| **Board #1** (RAK4631, marked "1") | Active dev board | Voltmeter incident Feb 20 → P0.31 grounded (recovered via SAADC workaround). **DAPLink dead**. NanoDAP only. |
| **Board #2** (RAK4631) | Active dev board | Pot overcurrent ~Jan 5. **DAPLink dead**. NanoDAP only. Intermittent SWD brownout during flash. |
| **RAK19007 baseboard ×2** | Working | 3 pins on J11: AIN1 (P0.31), IO1 (P0.17), IO2 (P1.02) |
| **RAK19001 baseboard** | Bad solder joints on even-row headers | 7+ IO pins. Even-row GPIOs work via back-side probe; front-side header pins have cold joints. |
| **RAK19011 baseboard** | Untested | Alternate baseboard. |
| **NanoDAP** | Working | Required for both boards (both DAPLinks dead). |

### Known Issues

- **Both DAPLinks Dead**: Neither board has a working onboard DAPLink. NanoDAP required for all flash operations.
- **Board #2 Power Brownout During Flash**: Red LED dims and flashes rapidly, pyOCD reports "No ACK." Must unsnap/resnap module. Best erase command: `pyocd erase --target nrf52840 --frequency 500000 --chip -Oconnect_mode=under-reset`.
- **Board #2 BLE Registration Failure**: Cannot complete Sidewalk BLE registration. PSA reboot loop with any cert. Requires NanoDAP for power — won't boot from USB alone. See TASK-113.
- **Duplicate Certificate Files**: Six cert JSONs were only 3 unique certs. Only `mfg5.hex` retained.

## Method

### Phase 1 — SAADC Latch Test (Board #1 on RAK19007)

**Step 1.1 — Resistance measurement**: SKIPPED (no multimeter available). Proceeded directly to firmware test.

**Step 1.2 — Firmware SAADC workaround build**: DONE
- Built firmware with `nrf_saadc_disable(NRF_SAADC)` at top of `platform_adc_init()`
- Flashed to Board #1 via NanoDAP. Branch `task/104-saadc-errata-workaround`, 454KB.
- Board boots, shell responds, Sidewalk init OK.
- Initial `app evse status` with nothing connected: Pilot voltage: 0 mV (expected — no pot).

**Step 1.3 — AIN1 ADC test**: DONE — **AIN1 IS ALIVE**
- Touched 3V3 (pin 5) to AIN1 (pin 22) on RAK19001 headers
- `app evse status` reported: **Pilot voltage: 3301–3343 mV**
- Pin bounced between 3300 mV and 0 mV as wire touched/released
- **RESULT: Hypothesis A confirmed** — SAADC mux latch was the cause — `nrf_saadc_disable()` released the latch

**Step 1.5 — Even-row GPIO test (all at once)**: DONE
- Built throwaway firmware on `exp/012-even-pin-test`
- Configured IO2, IO4, IO6 as pull-down inputs; added `app io read` shell command
- Touched 3V3 to each even-row pin:

| Pin | WisBlock Label | nRF52840 | Result |
|-----|---------------|----------|--------|
| 22 | AIN1 | P0.31 | **3301–3343 mV** — PASS |
| 30 | IO2 | P1.02 | LOW — **FAIL** |
| 32 | IO4 | P0.04 | LOW — **FAIL** |
| 38 | IO6 | P0.10 | LOW — **FAIL** |

Only pin 22 makes contact via front-side headers.

**Step 1.6 — Back-side probe**: DONE — **ROOT CAUSE IDENTIFIED**
- Probed even-row pins from back of RAK19001 PCB (bypassing soldered header pins)
- **All even-row pins work from the back** — IO2, IO4, IO6 all read HIGH
- **Root cause**: Cold/bad solder joints on the hand-soldered even-row header pins. Hirose connector and nRF52840 GPIOs are both fine.

### Phase 2 — RAK19001 Connector Validation

> **SUPERSEDED**: Step 1.6 identified actual root cause: bad solder joints on even-row header pins. Hirose DF40C connector is fine. Fix: reflow/resolder even-row header pins.

### Phase 3 — Final Pin Assignment Decision

**Outcome**: AIN1 recovered + Connector fine (bad solder)

Decisions:
1. **Use original pin map** — AIN7/P0.31 for pilot voltage
2. **Keep SAADC workaround permanently** — production requirement
3. **Reflow RAK19001 header pins when needed** — 10-minute soldering task for v1.1 features
4. **RAK19007 is sufficient for now** — has AIN1, IO1, IO2 for current v1.0 firmware

## Key Insights

- The SAADC analog mux latch is a real and dangerous failure mode. Any board reset during an active ADC sample can ground an analog pin until the next firmware flash with the workaround.
- Back-side PCB probing is a powerful technique for distinguishing connector/solder issues from silicon issues.
- Both boards have dead DAPLinks from separate incidents — the NanoDAP is a single point of failure for all development.

## References

- Branch: `task/104-saadc-errata-workaround`
- Files: `app/rak4631_evse_monitor/src/app.c` (`platform_adc_init`)
- Tasks: TASK-104, TASK-113
- Experiments: EXP-009b, EXP-010
