# TASK-060: Uplink payload v0x08 — remove pilot voltage, remove heat call

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: L (8 points)

## Description

Update the uplink payload from v0x07 (12 bytes) to v0x08 (10 bytes). Two changes:

1. **Remove pilot voltage** (bytes 3-4) from the uplink payload. The J1772 state enum (byte 2) is sufficient — raw millivolts are not needed in the cloud. Pilot voltage continues to be read from ADC AIN0 for on-device state classification (A-F) and displayed in `app evse status`, but is no longer transmitted.

2. **Remove heat call entirely from v1.0** — stop reading GPIO P0.04, remove `THERMOSTAT_FLAG_HEAT`, reserve bit 0 of the flags byte (always 0). The heat call GPIO is physically wired but not used until heat pump support is added. This affects firmware, selftest, shell output, Lambda decoding, and tests.

3. **Bump version byte** to 0x08. The decode Lambda must handle v0x07 and v0x08.

### New v0x08 payload layout (10 bytes)
```
Byte 0:    0xE5 (magic)
Byte 1:    0x08 (version)
Byte 2:    J1772 state (enum 0-6)
Byte 3-4:  Current draw (uint16_le, milliamps)
Byte 5:    Flags (bit 0 reserved/always 0, bit 1 COOL, bit 2 CHARGE_ALLOWED, bits 3-7 unchanged)
Byte 6-9:  Timestamp (uint32_le, SideCharge epoch)
```

### AC supply voltage assumption
AC supply voltage is assumed to be 240V for any power/energy calculations. The device has no line voltage sensing.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria

### Firmware — pilot voltage removal
- [ ] `event_snapshot` struct (`event_buffer.h`): remove `pilot_voltage_mv` field
- [ ] `event_buffer_add()` / serialization: update for new struct layout
- [ ] `app_entry.c`: stop storing pilot_voltage_mv in event snapshots
- [ ] `app_tx.c` / `evse_payload.c`: build v0x08 payload (10 bytes, no pilot voltage)
- [ ] Version byte set to 0x08
- [ ] `app evse status` shell command still shows pilot voltage (local diagnostic only)

### Firmware — heat call removal (74 references across codebase)
**platform_api_impl.c** (platform layer GPIO driver):
- [ ] Remove `HEAT_CALL_NODE` / `heat_call_gpio` definition (lines 83-87)
- [ ] Remove heat call GPIO initialization in `platform_gpio_init()` (lines 109-113)
- [ ] Remove heat call reading in `platform_gpio_get()` case for pin 1 (lines 276-279)

**thermostat_inputs.h**:
- [ ] Remove `#define THERMOSTAT_FLAG_HEAT (1 << 0)`
- [ ] Remove `bool thermostat_heat_call_get(void)` declaration

**thermostat_inputs.c**:
- [ ] Remove `#define EVSE_PIN_HEAT 1`
- [ ] Remove `thermostat_heat_call_get()` function
- [ ] Remove heat flag from `thermostat_flags_get()` — only return COOL bit

**selftest.c / selftest.h**:
- [ ] Remove `bool gpio_heat_ok` from `selftest_boot_result_t` struct
- [ ] Remove GPIO heat readable check from `selftest_boot()` (step 3)
- [ ] Remove heat from `selftest_all_passed()` condition
- [ ] Remove heat from selftest shell output ("GPIO heat: PASS/FAIL")

**selftest_trigger.c**:
- [ ] Remove heat from passed_count logic

**app_entry.c** (shell + logging):
- [ ] Remove "Heat: ON/OFF" from `app hvac status` shell output
- [ ] Remove `heat=%d` from thermostat change log message

### AWS
- [ ] `decode_evse_lambda.py`: add v0x08 decoder with new byte offsets (no pilot voltage, no heat)
- [ ] v0x07 decoder remains for backward compatibility
- [ ] Remove `thermostat_heat` from v0x08 decoded output
- [ ] Remove `thermostat_heat_active` from DynamoDB item for v0x08
- [ ] `terraform apply` to deploy updated Lambda

### Tests — C (firmware)
- [ ] `test_event_buffer.c`: update for new struct (no pilot_voltage_mv)
- [ ] `test_evse_sensors.c`: update if affected
- [ ] Payload serialization tests: update for v0x08 format
- [ ] `test_thermostat_inputs.c`: remove `test_heat_call_high`, `test_heat_call_low`, `test_flags_heat_only`
- [ ] `test_app.c`: remove/update all `mock_gpio_values[1]` heat setup lines; remove `test_thermostat_heat_only`, `test_selftest_boot_gpio_heat_fail`
- [ ] `test_selftest_trigger.c`: remove heat GPIO setup, update pass/fail count expectations
- [ ] `test_app_tx.c`: remove heat from mock GPIO setup, update flag assertions (0x07 → 0x06, etc.)
- [ ] `test_shell_commands.c`: remove `test_hvac_status_heat_on`, update heat-related assertions

### Tests — Python (AWS)
- [ ] `test_decode_evse.py`: add v0x08 decode tests
- [ ] Remove `thermostat_heat` assertions from v0x08 tests
- [ ] Verify v0x07 backward compat still passes (heat present in old payloads)
- [ ] All tests pass

## Testing Requirements
- [ ] `make -C rak-sid/app/rak4631_evse_monitor/tests/ clean test` — all pass
- [ ] `python3 -m pytest rak-sid/aws/tests/ -v` — all pass
- [ ] On-device: `app evse status` still shows pilot voltage locally
- [ ] On-device: `app hvac status` shows only cool call (no heat)
- [ ] On-device: uplink verified as 10 bytes via `decode_evse_lambda.py` logs

## Deliverables
- Updated firmware: v0x08 payload, no pilot voltage in uplink, no heat call reading
- Updated Lambda: v0x08 + v0x07 backward compat
- Updated tests: C and Python (heat tests removed, v0x08 tests added)
- Terraform deployed
