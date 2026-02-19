# TASK-097: Firmware v1.1 — Heat call input + interlock + uplink HEAT flag

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
Enable heat call (P0.04) as an active input in firmware. Read the GPIO, use it as an interlock trigger (same as cool call), and report it in the uplink payload flags byte.

### Work items
1. **Read heat call GPIO** (P0.04) in `thermostat_inputs.c` — add `thermostat_heat_call_get()` function
2. **Interlock trigger**: In `charge_control.c`, treat heat call rising edge the same as cool call — pause EV charging if heat call goes active
3. **Boot sequence**: Update `charge_control_init()` to read BOTH P0.04 and P0.05; block charging if either is active
4. **Uplink flags byte**: Set bit 0 (HEAT flag, 0x01) when heat call is active. Currently reserved/always 0.
5. **Shell commands**: Update `app hvac status` to show heat call state. Update `app evse status` if it references thermostat state.
6. **Change detection**: Heat call state change should trigger an uplink (same as cool call in rate limiter)
7. **LED behavior**: Heat call active should trigger the same LED pattern as cool call (compressor has priority)

### Payload change
Byte 7 (flags), bit 0: `0x01` = HEAT (heat call active). This was reserved in v0x08/v0x09 — now active. No payload version bump needed (bit was already allocated).

## Dependencies
**Blocked by**: TASK-096 (W-out relay GPIO must exist first)
**Blocks**: TASK-098

## Acceptance Criteria
- [ ] `thermostat_heat_call_get()` reads P0.04 correctly
- [ ] Heat call rising edge pauses EV charging (same behavior as cool call)
- [ ] Boot reads both GPIOs; either active → charge blocked
- [ ] Uplink flags byte bit 0 reflects heat call state
- [ ] Heat call state change triggers uplink
- [ ] `app hvac status` shows both cool call and heat call
- [ ] LED pattern correct for heat call active

## Testing Requirements
- [ ] Unit test: `thermostat_heat_call_get()` returns correct value
- [ ] Unit test: heat call triggers charge pause
- [ ] Unit test: boot blocks charging when heat call active
- [ ] Unit test: flags byte bit 0 set correctly in payload
- [ ] On-device: verify with `app hvac status` and uplink inspection

## Deliverables
- Updated `thermostat_inputs.c` / `.h`
- Updated `charge_control.c`
- Updated `evse_payload.c` (flags byte)
- Updated shell commands
- Unit tests
