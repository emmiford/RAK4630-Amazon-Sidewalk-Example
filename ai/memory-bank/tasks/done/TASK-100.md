# TASK-100: Remap firmware pin assignments to RAK19007 + RAK4631 WisBlock connector

**Status**: merged done (2026-02-19, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: `task/100-wisblock-pin-remap`
**Size**: M (5 points)

## Summary

Remapped all firmware GPIO/ADC pins from raw nRF52840 (P0.02–P0.07) to the 3 accessible RAK19007 J11 header pins. Dropped current clamp, heat call, and Charge Now button (no pins available). Re-applied TASK-065 charge_block polarity fix (LOW=allowed, HIGH=blocked). Fixed IO2 cool_call to GPIO_ACTIVE_LOW to match RAK19007 3V3_S external pull-up.

### Final pin mapping

| WisBlock (J11) | nRF52840 | Signal | Direction | GPIO flags |
|---|---|---|---|---|
| AIN1 | P0.31 / AIN7 | Pilot voltage | ADC input | — |
| IO1 | P0.17 | charge_block | Output | GPIO_ACTIVE_HIGH |
| IO2 | P1.02 | cool_call | Input | GPIO_PULL_UP, GPIO_ACTIVE_LOW |

### IO2 / 3V3_S resolution
RAK19007 has an external pull-up on P1.02 for the 3V3_S power rail. Resolved by using GPIO_ACTIVE_LOW + GPIO_PULL_UP so the Zephyr driver inverts the reading. No driver conflict observed during testing.

## Changes (17 files, +305 / -343 lines)

### Firmware (7 files)
- `rak4631_nrf52840.overlay` — 3 pins only, removed heat_call/charge_now/channel@1, cool_call GPIO_ACTIVE_LOW
- `platform_api_impl.c` — removed GPIO_PIN_1 (heat), GPIO_PIN_3 (charge_now), reduced ADC to 1 channel
- `evse_sensors.c` — current clamp stubbed to return 0 mA
- `selftest.c` — reduced from 4 to 3 checks, charge_en→charge_block rename
- `selftest_trigger.c` — SELFTEST_CHECK_COUNT 4→3, charge_block rename
- `selftest_trigger.h` — check count updated
- `selftest.h` — removed adc_current_ok, renamed charge_en_ok→charge_block_ok
- `charge_control.c` — polarity inversion: `allowed ? 0 : 1` (LOW=allow, HIGH=block)

### Tests (5 files)
- `test_evse_sensors.c` — replaced current clamp tests with single stub test
- `test_app.c` — updated selftest, charge control, delay window GPIO assertions
- `test_app_tx.c` — current expects 0
- `test_selftest_trigger.c` — 3 checks, updated blink counts
- `test_shell_commands.c` — polarity-flipped GPIO assertions, current expects 0 mA
- `test_charge_control.c` — polarity-flipped init/allow/pause GPIO assertions

### Documentation (3 files)
- `docs/technical-design.md` — 18+ sections: pin tables, relay architecture (NC), power loss = transparent = safe
- `docs/PRD.md` — WisBlock silkscreen names as primary identifiers, NC relay architecture, fail-safe rewrite
- `docs/lexicon.md` — updated Cp Pin, Circuit Interlock, Mutual Exclusion, Charge Now Override

## Test Results
- 15/15 C unit tests pass
- 326/326 Python tests pass
- On-device: selftest ALL PASS, LED toggles on IO1, button reads on IO2, pilot ADC reads 249mV
