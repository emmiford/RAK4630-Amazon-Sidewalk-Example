# EXP-009b: Board #2 Potentiometer Overcurrent Incident

**Status**: Concluded
**Verdict**: DOCUMENTED
**Type**: Incident report
**Date**: ~2026-01-05
**Owner**: Oliver
**Related**: EXP-010, EXP-012, TASK-113

---

## Problem Statement

Board #2 (RAK4631, RAK19007 baseboard) became partially unflashable after a potentiometer connected to the J11 header started to smoke. The board's USB/DAPLink interface died, and it exhibits intermittent SWD failures. Understanding the damage mechanism and extent is necessary for recovery planning.

## Incident Details

**Board**: Board #2 (RAK4631). RAK19007 baseboard (only 1 analog pin on J11: AIN1/P0.31).
**Firmware at time of incident**: Original RAK SDK demo (rak1901 temperature/humidity). No custom ADC or GPIO configuration. All pins in nRF52840 default reset state (disconnected input, high impedance).

A small potentiometer was connected to the RAK19007 J11 header with protection resistors on both sides. The pot was turned up and started to smoke. Shortly thereafter, the board became unable to flash via its built-in USB/DAPLink interface.

## Damage Assessment

| Capability | Status | Evidence |
|-----------|--------|----------|
| Flash memory | **Working** | MFG, platform, and app all flashed via NanoDAP |
| SWD debug port | **Intermittent** | Drops connection constantly, even with stable wiring. Dozens of retries needed. |
| USB / DAPLink | **Dead** | Cannot flash or connect via USB. NanoDAP required. |
| AIN1 (P0.31) | **Possibly damaged** | Measured as shorted to ground (unconfirmed — may be floating input). |
| BLE registration | **Not attempted** | Never completed first-boot BLE registration. |

## Root Cause Analysis

**The pot smoked because of excessive power dissipation through the pot's resistive element.** The MCU pin (P0.31, unconfigured high-impedance input) could not source or sink enough current to smoke anything (~5mA max from GPIO, and pin wasn't configured as output). The current path was 3V3 from J11 header → pot → GND.

For a pot to smoke at 3.3V, total resistance must be very low:
- 100Ω → 109mW (borderline for small trim pot rated 100-250mW)
- 50Ω → 218mW (smoking)
- 20Ω → 545mW (definitely smoking)

**The "coded as output" theory is ruled out.** On Jan 5, firmware was the RAK SDK demo with no custom pin config. P0.31 was in nRF52840 default reset state. No firmware version ever configured P0.31 as GPIO output.

### Why the Board Became Partially Unflashable

**Theory 1 — LDO thermal damage**: RAK19007's onboard voltage regulator was trying to supply the short-circuit current through the pot. Sustained overcurrent overheated the LDO, permanently degrading it. Now produces marginal voltage on power-up → intermittent SWD, dead USB.

**Theory 2 — nRF52840 latch-up**: If the 3V3 rail sagged during the overcurrent and then recovered rapidly, the voltage transient could trigger CMOS latch-up. Damage pattern: flash memory survives (robust oxide), sensitive analog/digital peripherals (USB PHY, SWD timing, SAADC) degraded.

Both theories are consistent with observed behavior.

## nRF52840 AIN Naming Confusion

A critical naming confusion affected the project from Feb 1 through Feb 19:

| Label | nRF52840 SAADC | Physical Pin | WisBlock Connector |
|-------|---------------|-------------|------------------------------|
| `NRF_SAADC_AIN0` | AIN0 | P0.02 | **Not on WisBlock connector** |
| `NRF_SAADC_AIN1` | AIN1 | P0.03 | **Not on WisBlock connector** |
| `NRF_SAADC_AIN7` | AIN7 | P0.31 | Pin 22, labeled "AIN1" on J11 |

The original firmware (commit `e35af07`, Feb 1) used `NRF_SAADC_AIN0` (P0.02) — not routed to the WisBlock connector. All telemetry before Feb 20 was floating-pin noise. Fixed by TASK-100 (commit `41ea3fe`, Feb 19).

## Recovery Path

Board #2 is partially functional but unreliable:
1. **Continue with NanoDAP**: Accept intermittent SWD, use as secondary test board
2. **Write off**: Treat as parts donor
3. **Power investigation**: Board requires NanoDAP for power — won't boot from USB alone (see EXP-012)

## Key Insights

- Trim pots at low resistance settings can exceed their power rating at 3.3V. Always check minimum resistance vs power dissipation.
- Component smoke is a power dissipation event, not necessarily an MCU fault.
- Pin naming confusion between nRF52840 native SAADC channels and WisBlock connector labels is a major gotcha.
