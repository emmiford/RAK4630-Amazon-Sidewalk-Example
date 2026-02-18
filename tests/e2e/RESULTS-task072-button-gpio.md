# TASK-072: Charge Now Button GPIO Verification

**Date**: 2026-02-17
**Firmware**: task/072-button-gpio-verify (app built from worktree)
**Tester**: Eero (automated + manual)
**Device**: RAK4631 on RAK5005-O baseboard

## Summary

Partial verification — GPIO pin reads correctly in idle state. Physical button
not yet wired to P0.07, so press detection and 5-press trigger cannot be tested.

## Code Change

Added `Button GPIO: <value>` readout to `app selftest` / `sid selftest` shell
output (`selftest.c`). This reads `EVSE_PIN_BUTTON` (GPIO index 3, mapped to
P0.07) and displays the current value.

## Test Results

| Check | Result | Notes |
|-------|--------|-------|
| `app selftest` shows Button GPIO line | PASS | New line: `Button GPIO: 0` |
| GPIO reads 0 when button not pressed | PASS | Pull-down active, pin at GND |
| GPIO reads 1 when button pressed | BLOCKED | No physical button wired |
| 5-press trigger fires selftest | BLOCKED | No physical button wired |
| LED blink codes display on trigger | BLOCKED | No physical button wired |
| Button wired P0.07 ↔ VDD | NOT DONE | Hardware assembly pending |

## Selftest Output (button not pressed / not wired)

```
=== Self-Test ===
  ADC pilot:     PASS
  ADC current:   PASS
  GPIO cool:     PASS
  Charge enable: PASS
  Button GPIO:   0
  J1772 state:   E (Error) (239 mV)
  Current:       2300 mA
  Clamp match:   WARN (mismatch)
  Interlock:     WARN (current while paused)
  Fault flags:   0x00
=== FAIL ===
```

Clamp/Interlock WARNs are expected on bench (floating ADC reads ~2300 mA with
no actual current, and charge is paused).

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
- GPIO_ACTIVE_HIGH: ready for button wired to VDD

## Next Steps

When physical button is wired (momentary switch between P0.07/WB_IO2 and VDD):
1. Re-run `app selftest` — should show `Button GPIO: 1` while held
2. Press 5 times within 5 seconds — should trigger selftest + LED blinks
3. Observe 4 green blinks (all hardware checks pass on bench, except clamp/interlock)
