# EXP-010: RAK19001 Baseboard Pin Mapping Validation

**Status**: Concluded
**Verdict**: DOCUMENTED
**Type**: Hardware diagnostic
**Date**: 2026-02-21
**Owner**: Oliver
**Related**: EXP-009b, EXP-012, REC-007, REC-008, REC-009

---

## Problem Statement

After switching from RAK19007 to RAK19001 WisBlock Dual IO Base Board, AIN1 (P0.31, pin 22) measured as grounded with a voltmeter. The RAK19001 was chosen to gain access to more analog pins. The question was whether the pin failure was caused by nRF52840 silicon errata, NFC pin conflict, a bad baseboard, or something mechanical.

## Hypothesis

The pilot ADC failure on P0.31 is caused by an nRF52840 SAADC errata that latches analog inputs to ground. Switching to AIN2 (P0.04/IO4) and adding the SAADC disable workaround at boot will restore analog input functionality.

**Success Metrics**:
- Primary: successful ADC reading of pilot voltage on the new pin
- Secondary: successful GPIO output and input on all available IO pins

## Method

**Type**: Systematic hardware bring-up — pin-by-pin validation using GPIO output (LED toggle), GPIO input (button/wire short), and ADC tests.
**Branch**: `task/ain2-swap` (worktree)

**Firmware Changes**:
1. ADC input changed from `NRF_SAADC_AIN7` (P0.31) to `NRF_SAADC_AIN2` (P0.04/IO4) in devicetree overlay
2. Added `nrf_saadc_disable(NRF_SAADC)` at start of `platform_adc_init()` as SAADC errata workaround
3. Added `CONFIG_NFCT_PINS_AS_GPIOS=y` to `prj.conf`
4. Various pin swaps for `charge_block` and `cool_call` during iterative testing

## Results

### GPIO Output (charge_block / LED toggle)

| Connector Pin | WisBlock Label | nRF52840 GPIO | Result | Row |
|:---:|:---:|:---:|:---:|:---:|
| 29 | IO1 | P0.17 | **PASS** | Odd |
| 30 | IO2 | P1.02 | **FAIL** | Even |
| 31 | IO3 | P0.21 | **PASS** | Odd |
| 32 | IO4 | P0.04 | **FAIL** | Even |
| 37 | IO5 | P0.09 | **PASS** | Odd |
| 38 | IO6 | P0.10 | **FAIL** | Even |
| 24 | IO7 | NC | **FAIL** (expected) | Even |

### GPIO Input (cool_call / button + wire short)

| Connector Pin | WisBlock Label | nRF52840 GPIO | Result | Method | Row |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 31 | IO3 | P0.21 | **PASS** | Button | Odd |
| 37 | IO5 | P0.09 | **PASS** | Wire short | Odd |
| 30 | IO2 | P1.02 | **FAIL** | Button | Even |
| 32 | IO4 | P0.04 | **FAIL** | Button | Even |
| 38 | IO6 | P0.10 | **FAIL** | Wire short | Even |

**Key discovery**: IO5 wire-short to GND worked immediately — proved the pin was good and the button assembly was broken/miswired.

### ADC

| Connector Pin | WisBlock Label | nRF52840 Analog | Result | Row |
|:---:|:---:|:---:|:---:|:---:|
| 22 | AIN1 | P0.31 / AIN7 | **FAIL** — 0mV | Even |
| 32 | IO4 | P0.04 / AIN2 | **FAIL** — 0mV | Even |

SAADC errata workaround applied but did not resolve ADC failure. Both analog pins on even-numbered connector positions.

### The Pattern

**Every failing pin is on an even-numbered connector position. Every passing pin is on an odd-numbered connector position.**

On the RAK19001's 40-pin board-to-board connector, odd and even pins sit on opposite physical rows. One entire row has no electrical contact.

**Decision**: ROOT CAUSE IDENTIFIED — Mechanical connector seating failure (later refined by EXP-012 Step 1.6 to: bad solder joints on hand-soldered even-row header pins).

## Theories Investigated and Ruled Out

| # | Theory | Evidence Against |
|---|--------|-----------------|
| 1 | nRF52840 SAADC errata | `nrf_saadc_disable()` didn't fix; failure affects digital GPIO, not just analog |
| 2 | NFC pin conflict | `NFCT_PINS_AS_GPIOS=y` didn't help; IO5 (odd) worked, IO6 (even) didn't |
| 3 | Individual dead pins | Perfect odd/even split across 7 pins rules out random failure |
| 4 | Bad baseboard traces | Manufacturing defect wouldn't produce perfect odd/even pattern |
| 5 | Broken button/pot | Secondary issue — wire-short tests bypassing button confirmed primary fault |

## Addendum (2026-02-22)

EXP-012 Step 1.6 (back-side probe) refined the root cause: **cold/bad solder joints on hand-soldered even-row header pins** on the RAK19001 baseboard. The Hirose DF40C connector and nRF52840 GPIOs are both fine. All telemetry before Feb 20 was floating-pin noise from the AIN naming confusion (see EXP-009b), not this hardware fault.

## Key Insights

- **Mechanical failures masquerade as electrical/software bugs.** The original symptom looked exactly like SAADC errata. Hours were spent on firmware workarounds before the physical root cause was identified.
- **Systematic pin-by-pin testing reveals patterns that point-testing misses.** Testing one pin gave no information about root cause. Testing seven pins revealed the odd/even pattern.
- **The "broken button" red herring was costly.** Wire-shorting IO5 directly to GND was the breakthrough that separated button problems from connector problems.
- **Board-to-board connectors are fragile.** Hirose DF40C 0.4mm pitch connectors require significant, even pressure to seat fully.
- **Always validate assumptions on new hardware before writing software workarounds.**
