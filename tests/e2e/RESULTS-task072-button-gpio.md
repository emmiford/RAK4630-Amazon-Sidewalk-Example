# TASK-072: Charge Now Button GPIO Verification

**Date**: 2026-02-17 (initial), 2026-02-22 (hardware verified)
**Firmware**: task/072b-button-verified
**Tester**: Eero (initial software), Emily (hardware verification)
**Device**: RAK4631 on RAK19007 baseboard

## Summary

Full verification complete. GPIO pin reads correctly in idle and pressed states.
Physical button wired to P0.07. 5-press selftest trigger confirmed working.

## Code Change

Added `Button GPIO: <value>` readout to `app selftest` shell output
(`selftest.c`). Reads `PIN_CHARGE_NOW_BUTTON` (GPIO index 3, mapped to P0.07)
and displays the current value.

## Test Results

| Check | Result | Notes |
|-------|--------|-------|
| `app selftest` shows Button GPIO line | PASS | `Button GPIO: 0` |
| GPIO reads 0 when button not pressed | PASS | Pull-down active, pin at GND |
| GPIO reads 1 when button pressed | PASS | Hardware verified 2026-02-22 |
| 5-press trigger fires selftest | PASS | Hardware verified 2026-02-22 |
| LED blink codes display on trigger | PASS | Hardware verified 2026-02-22 |
| Button wired P0.07 ↔ VDD | PASS | Momentary switch installed |

## Devicetree Confirmation

Pin configuration from `rak4631_nrf52840.overlay`:
```
charge_now_button {
    compatible = "gpio-keys";
    charge_now: charge_now {
        gpios = <&gpio0 7 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
        label = "Charge Now Button";
    };
};
```

- GPIO_PULL_DOWN: confirmed (reads 0 when floating/unwired)
- GPIO_ACTIVE_HIGH: confirmed (reads 1 when button pressed to VDD)
