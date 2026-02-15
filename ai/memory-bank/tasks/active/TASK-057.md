# TASK-057: Route selftest through evse_sensors, not direct ADC

**Status**: not started
**Priority**: P2
**Owner**: Eero
**Branch**: —
**Size**: S (2 points)

## Description
`selftest_continuous_tick()` reads ADC directly via `api->adc_read_mv(0)` instead of using `evse_sensors_j1772_state_get()`. This means:
- Simulation mode doesn't affect selftest (inconsistent behavior during shell testing)
- Two code paths interpret the same ADC channel (maintenance risk)
- selftest has implicit knowledge of ADC channel numbering

Change the function signature so `app_entry.c` passes sensor readings (which it already has from the polling loop) instead of selftest reading ADC directly:

```c
/* Before */
void selftest_continuous_tick(j1772_state_t state, int current_ma,
                              bool charge_allowed);
// Internally reads: api->adc_read_mv(0) for pilot_mv

/* After */
void selftest_continuous_tick(j1772_state_t state, int pilot_mv,
                              int current_ma, bool charge_allowed,
                              uint8_t thermostat_flags);
// Uses only what it's given — no ADC knowledge needed
```

After this change, selftest no longer needs a platform API pointer for continuous monitoring (boot selftest still needs it for GPIO toggle-and-verify).

Reference: `docs/technical-design-rak-firmware.md`, Change 8.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] `selftest_continuous_tick()` does not call `api->adc_read_mv()` directly
- [ ] `app_entry.c` passes pilot_mv and thermostat_flags from its polling loop
- [ ] selftest.h updated with new signature
- [ ] All continuous fault monitors still function (clamp mismatch, interlock, pilot fault, thermostat chatter)
- [ ] Simulation mode (`app evse a/b/c`) is visible to selftest continuous monitoring

## Testing Requirements
- [ ] All 57 host-side tests pass (update affected test cases for new signature)
- [ ] New test: continuous_tick uses provided pilot_mv, not ADC read
- [ ] Existing selftest tests updated to pass pilot_mv and thermostat_flags

## Deliverables
- Modified: `src/app_evse/selftest.c` (remove internal ADC read from continuous_tick)
- Modified: `include/selftest.h` (updated function signature)
- Modified: `src/app_evse/app_entry.c` (pass additional values to continuous_tick)
- Modified: `tests/test_app.c` (update selftest test cases)
