# Self-Test E2E Results — TASK-048

**Date**: 2026-02-17
**Firmware**: task/048-selftest-verify-stale-flash @ 66f5b6f
**Tester**: Eero (automated)

## Boot Self-Test

| Check | Result | Notes |
|-------|--------|-------|
| adc_pilot | PASS | ADC channel 0 readable |
| adc_current | PASS | ADC channel 1 readable |
| gpio_heat | PASS | GPIO input readable |
| gpio_cool | PASS | GPIO input readable |
| charge_en | FAIL | nRF52840 input buffer disconnected — readback always 0. Fix: GPIO_OUTPUT_ACTIVE \| GPIO_INPUT |
| Error LED pattern | N/A | "LED index out of range" from dk_buttons_and_leds (LED 2 not mapped on RAK4631) |

Serial log excerpt (boot):
```
No selftest failure log line at boot (expected: selftest_boot() runs silently on pass).
Charge_en FAIL triggers FAULT_SELFTEST but no boot-time log message is printed for that.
```

## Shell Self-Test (`sid selftest`)

```
=== Self-Test ===
  ADC pilot:     PASS
  ADC current:   PASS
  GPIO heat:     PASS
  GPIO cool:     PASS
  Charge enable: FAIL
  J1772 state:   Unknown (0 mV)
  Current:       2454 mA
  Clamp match:   WARN (mismatch)
  Interlock:     WARN (current while paused)
  Fault flags:   0xff
=== FAIL ===
```

Result: FAIL — but only charge_en is a real bug. Other results are expected for no-EVSE-connected bench test:
- J1772 Unknown (0 mV): No pilot signal without an EVSE charger
- Current 2454 mA: Floating ADC input noise (~270 mV × 30000/3300 calibration)
- Clamp/Interlock WARN: Follow from the spurious current reading
- Fault flags 0xFF: Expected 0xF0 (SELFTEST|SENSOR|CLAMP|INTERLOCK). Lower nibble 0x0F is anomalous — investigate BSS initialization of app static variables.

### Charge Enable Root Cause

`platform_api_impl.c:101` configures `charge_en_gpio` with `GPIO_OUTPUT_ACTIVE` only. On nRF52840, this disconnects the input buffer (PINDIR=1, INPUT=1=disconnected). The selftest toggle-and-verify calls `gpio_pin_get_dt()` which reads NRF_GPIO->IN — returns 0 regardless of output state.

**Fix applied**: Changed to `GPIO_OUTPUT_ACTIVE | GPIO_INPUT` to keep input buffer connected for readback. Requires platform rebuild + reflash to verify.

### LED Error

`dk_buttons_and_leds` log: "LED index out of the range". The selftest calls `api->led_set(2, ...)` on failure, but RAK4631 only has LED 0 and LED 1 mapped. Non-blocking (just a cosmetic log error).

## Fault Flag Uplink

### Trigger: Clamp mismatch (State C, no load)
- Simulated via: N/A (not tested — requires platform rebuild first for clean selftest baseline)
- Wait time: N/A
- Uplink byte 7 hex: N/A
- DynamoDB fields: N/A

### Clear: Return to State A
- N/A (blocked on platform rebuild)

## Stale Flash Erase (TASK-022)

- Flash.sh `flash_app()` now erases partition (0x90000-0xCEFFF) before writing
- `ota_update.c` erases stale pages beyond image end after OTA apply
- `ota_deploy.py` warns if baseline dump >> app binary size
- Device verification:
  - App rebuilt from 4524 bytes (stale, pre-selftest) to 8372 bytes (current)
  - Partition erased with `pyocd erase --sector 0x90000+0x3F000` before flashing
  - pyocd `cmd -c "erase ..."` does NOT work (flash init timeout) — must use `pyocd erase --sector`
  - `pyocd flash --no-erase` flag does NOT exist — removed from flash.sh

## Summary

| Test | Pass/Fail | Notes |
|------|-----------|-------|
| Boot self-test | PARTIAL | 4/5 checks pass. charge_en FAIL due to GPIO config bug (fix applied, needs rebuild) |
| Shell self-test | PARTIAL | Same charge_en issue. Cross-checks expected to fail without EVSE hardware |
| Fault flags in uplink | BLOCKED | Needs platform rebuild with GPIO fix for clean baseline |
| Fault flags in DynamoDB | BLOCKED | Same |
| Fault flags clear on resolve | BLOCKED | Same |
| Stale flash erase (code) | PASS | Three-layer defense committed and tested |
| Stale flash erase (pyocd) | PASS | `pyocd erase --sector` verified working on device |
| flash.sh fix | PASS | Fixed: PYOCD variable, `erase --sector`, removed `--no-erase` |

**Overall**: PARTIAL — Code changes verified. Charge enable GPIO fix requires platform rebuild + reflash to complete device verification. Fault flag uplink/DynamoDB tests blocked on clean selftest baseline.
