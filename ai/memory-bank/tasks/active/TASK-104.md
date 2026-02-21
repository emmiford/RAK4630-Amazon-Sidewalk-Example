# TASK-104: Evaluate SAADC errata workaround retention

**Status**: not started
**Priority**: P3
**Owner**: —
**Branch**: —
**Size**: S (1 point)

## Description
EXP-010 added `nrf_saadc_disable(NRF_SAADC)` at the start of `platform_adc_init()` as a workaround for the nRF52840 SAADC errata (analog mux latching inputs to ground). The workaround did not fix the EXP-010 failure (root cause was mechanical connector seating), but the underlying SAADC errata is real and could manifest in other scenarios such as warm reboot, long uptime, or sleep/wake cycles.

Evaluate whether this 2-line defensive workaround should be kept permanently or removed to avoid cargo-cult code. Review the specific nRF52840 SAADC errata (anomaly 86, 87, or related) and determine if the disable-at-boot pattern is the recommended Nordic mitigation.

Reference: Oliver's experiment log EXP-010 and recommendation REC-008.

## Dependencies
**Blocked by**: none
**Blocks**: none

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

## Deliverables
- Decision documented (keep or remove) with errata reference
- Code updated accordingly (comment added or workaround removed)
