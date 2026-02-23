# TASK-104: Apply SAADC errata workaround + validate AIN1 recovery

**Status**: MERGED DONE (2026-02-22, Eliel)
**Priority**: P1
**Branch**: `task/104-saadc-errata-workaround` → merged to main

## Summary
Added `nrf_saadc_disable(NRF_SAADC)` at boot before ADC init to release analog mux latch on AIN1 (P0.31). AIN1 recovered: 3301–3343 mV. Fixed pre-existing `leds_id_t` typedef build error. SAADC workaround is now a permanent production safety measure.

## Deliverables
- `platform_api_impl.c`: `#include <hal/nrf_saadc.h>` + `nrf_saadc_disable()` call in `platform_adc_init()`
- `app_leds.c`: `leds_id_t` typedef fix
- Commits: `2b56d8c`, `bc3cceb`
