# TASK-096: Firmware — W-out relay GPIO + platform API update

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
Add GPIO support for the W-out (heat call pass-through) relay in the platform layer. This is a v1.0 hardware enablement task — the GPIO pin must be assignable and controllable even though firmware heat call logic is v1.1 (TASK-097).

### Work items
1. **Assign GPIO pin** for W-out relay (pin TBD from PCB design, TASK-095)
2. **Add pin definition** to device tree overlay (`rak4631_nrf52840.overlay`) and `platform_api.h`
3. **Add pin enum** (e.g., `EVSE_PIN_HEAT_BLOCK`) to the pin constants
4. **Initialize GPIO** in platform init (configure as output, default de-energized = blocked)
5. **Shell command**: Add `sid relay` or extend `app hvac` to manually toggle W-out relay for testing
6. **Update hardware interlock logic** if the interlock is partially software-driven: ensure W-out cannot be energized while charge is active

### Design constraint
The W-out relay de-energized state = heat call blocked. This is safe-by-default: on power loss, both compressor calls are blocked. The firmware must explicitly energize W-out to allow heat call pass-through (same pattern as Y-out).

## Dependencies
**Blocked by**: TASK-094, TASK-095 (need GPIO pin assignment from PCB)
**Blocks**: TASK-097

## Acceptance Criteria
- [ ] W-out relay GPIO defined in overlay and platform_api.h
- [ ] GPIO initialized as output, default LOW (blocked)
- [ ] Shell command to toggle W-out relay for commissioning/debug
- [ ] Existing Y-out and charge block relay behavior unchanged
- [ ] Unit tests for new GPIO init path

## Testing Requirements
- [ ] Host-side unit test: W-out GPIO initialized correctly
- [ ] Host-side unit test: interlock prevents W-out + charge simultaneously
- [ ] On-device: `app hvac status` shows W-out relay state

## Deliverables
- Updated `rak4631_nrf52840.overlay`
- Updated `platform_api.h` (new pin constant)
- Updated platform init code
- Unit tests
