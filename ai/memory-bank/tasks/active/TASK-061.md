# TASK-061: Event buffer — write on state change, not every poll cycle

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: S (2 points)

## Description

The event buffer (TASK-034) currently snapshots EVSE state every 500ms poll cycle, generating ~120 entries/minute. Most entries are identical (state hasn't changed). This wastes buffer space and uplink bandwidth. Instead, write an event only when a field actually changes (J1772 state transition, voltage threshold crossed, charge control state change, thermostat flag change).

### What needs to happen

1. **Compare current state to last-buffered state** before writing a new entry
2. **Write only on meaningful change**: J1772 state, voltage band (not noise), current threshold, charge control state, thermostat flags
3. **Periodic heartbeat entry**: Write at least one entry per N minutes even if nothing changed, so the cloud knows the device is alive and sampling
4. **Preserve existing buffer API**: `event_buffer_write()` signature unchanged; the change-detection logic wraps the call site in `app_entry.c`

## Dependencies

**Blocked by**: none (event buffer TASK-034 is merged)
**Blocks**: none

## Acceptance Criteria

- [ ] Event buffer entries written only on state change (not every poll)
- [ ] Heartbeat entry written at least once per configurable interval (e.g., 5 min)
- [ ] Voltage changes below a noise threshold (e.g., ±2V) do not trigger a new entry
- [ ] J1772 state transitions always trigger a new entry
- [ ] Charge control state changes always trigger a new entry
- [ ] Buffer lifetime extended significantly (measured: entries/hour before vs. after)

## Testing Requirements

- [ ] C unit test: no new entry when state unchanged
- [ ] C unit test: new entry when J1772 state changes
- [ ] C unit test: heartbeat entry after timeout with no changes
- [ ] C unit test: voltage noise below threshold does not trigger entry

## Deliverables

- Modified `app/rak4631_evse_monitor/src/app_evse/app_entry.c` — change-detection wrapper
- Possibly new `event_filter.c` / `event_filter.h` if logic is non-trivial
