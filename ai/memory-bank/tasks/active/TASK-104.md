# TASK-104: Apply SAADC errata workaround + validate AIN1 recovery

**Status**: validated on device (2026-02-22, Eliel) — ready to merge
**Priority**: P1 (elevated from P3, 2026-02-22)
**Owner**: Eliel
**Branch**: `task/104-saadc-errata-workaround`
**Size**: S (1 point)

## Description
Board #1's AIN1 pin (P0.31) has read 0mV since a board reset during a voltmeter probe on Feb 20. The leading theory is that the reset interrupted an active SAADC sample, latching the analog mux to an internal ground reference (known nRF52840 errata behavior). Adding `nrf_saadc_disable(NRF_SAADC)` at boot should release the latch and recover the pin.

This is EXP-012 Phase 1 — the first step in the pin recovery experiment. If the workaround recovers AIN1, it becomes a permanent production requirement and this task is the implementation vehicle.

If AIN1 does NOT recover, the pin has ESD damage and is permanently dead — the workaround should still be evaluated for defensive value against future latch events (see REC-008).

**Pre-step (physical, no firmware)**: Measure resistance from P0.31 to GND on the unpowered Board #1 module. If near 0Ω, the ESD diode is blown and the firmware fix cannot help — skip directly to EXP-012 Phase 2.

Reference: Oliver's experiment log EXP-012 Phase 1, EXP-010 addendum, REC-008.

## Dependencies
**Blocked by**: none
**Blocks**: EXP-012 Phase 2 decision (determines if AIN1 is available on RAK19001)

## Acceptance Criteria
- [ ] Identify the specific nRF52840 SAADC errata number(s) relevant to analog mux latching
- [ ] Determine if `nrf_saadc_disable()` at boot is the Nordic-recommended mitigation for the identified errata
- [ ] If yes: keep the workaround, add a code comment citing the errata number
- [ ] If no: remove the workaround and document the rationale
- [ ] Optional stress test: 1000 rapid ADC enable/disable cycles + sleep/wake transitions, verify no stuck-at-ground readings with and without the workaround

## Testing Requirements
- [ ] ADC accuracy verified after stress test (if performed)
- [ ] Existing unit tests and CI pass with the chosen approach

## Implementation Reference (from EXP-010 session, not committed)

**File**: `app/rak4631_evse_monitor/src/platform_api_impl.c`

1. Add include after `<zephyr/drivers/adc.h>`:
```c
#include <hal/nrf_saadc.h>
```

2. Add at the top of `platform_adc_init()`, before the `if (adc_initialized)` check:
```c
/* nRF52840 SAADC errata workaround: the analog mux can latch pins
 * to ground across reboot cycles. Force-disable the peripheral to
 * release any latched pins before re-initializing. */
nrf_saadc_disable(NRF_SAADC);
```

## On-Device Validation (2026-02-22)
- **Result**: AIN1 (P0.31) reads **3301–3343 mV** with 3V3 applied — **fully recovered**
- **Hypothesis A confirmed**: SAADC analog mux latch was the cause, not ESD damage
- **Decision**: Keep `nrf_saadc_disable()` workaround permanently as production safety measure
- **Bonus**: Even-row connector pin 22 also works, suggesting EXP-010 connector seating issue resolved
- **Commits**: `2b56d8c` (SAADC workaround) + `bc3cceb` (leds_id_t pre-existing fix)

## Remaining Before Merge
- [ ] Identify specific nRF52840 SAADC errata number and add to code comment (can be done post-merge)
- [x] On-device validation — AIN1 recovered
- [ ] Run CI tests (host-side C tests + Python Lambda tests)

## Deliverables
- Decision documented (keep or remove) with errata reference
- Code updated accordingly (comment added or workaround removed)
