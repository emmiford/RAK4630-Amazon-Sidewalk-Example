# TASK-057: Route selftest through evse_sensors, not direct ADC

**Status**: MERGED DONE (2026-02-18, Eero)
**Priority**: P2
**Branch**: `task/057-selftest-evse-sensors` (merged to main)
**Size**: S (2 points)

## Summary
Removed direct `api->adc_read_mv(0)` call from `selftest_continuous_tick()`. Pilot fault
detection now uses the `j1772_state` passed by `app_entry.c` (from `evse_sensors`), so
simulation mode is visible to selftest and there is only one code path interpreting the
pilot ADC. Changed last parameter from `bool cool_call` to `uint8_t thermostat_flags` so
`app_entry.c` passes the flags it already has from its polling loop. Added
`test_continuous_pilot_uses_state_not_adc`. 136/136 C tests pass.
