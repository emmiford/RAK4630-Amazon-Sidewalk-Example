# TASK-060: Uplink payload v0x08 — reserve heat flag, keep pilot voltage

**Status**: in progress (2026-02-17, Eliel) — implementation committed, ready for merge
**Priority**: P1
**Owner**: Eliel
**Branch**: task/060-uplink-v08
**Size**: M (5 points)

## Description

Update the uplink payload from v0x07 to v0x08. **One substantive change**:

1. **Remove heat call entirely from v1.0** — stop reading GPIO P0.04, remove `THERMOSTAT_FLAG_HEAT`, reserve bit 0 of the flags byte (always 0). The heat call GPIO is physically wired but not used until heat pump support is added. This affects firmware, selftest, shell output, Lambda decoding, and tests.

2. **Bump version byte** to 0x08. The decode Lambda must handle v0x07 and v0x08.

**Pilot voltage is RETAINED in the uplink** (bytes 3-4, same as v0x07). The raw millivolt reading enables cloud-side detection of marginal pilot connections — readings near a threshold boundary indicate a degraded connection that the state enum alone would not reveal. See ADR-004. The payload remains 12 bytes.

### v0x08 payload layout (12 bytes, same size as v0x07)
```
Byte 0:    0xE5 (magic)
Byte 1:    0x08 (version)
Byte 2:    J1772 state (enum 0-6)
Byte 3-4:  Pilot voltage (uint16_le, millivolts)
Byte 5-6:  Current draw (uint16_le, milliamps)
Byte 7:    Flags (bit 0 reserved/always 0, bit 1 COOL, bit 2 CHARGE_ALLOWED, bits 3-7 unchanged)
Byte 8-11: Timestamp (uint32_le, SideCharge epoch)
```

### AC supply voltage assumption
AC supply voltage is assumed to be 240V for any power/energy calculations. The device has no line voltage sensing.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria

### Docs (completed on branch task/060-uplink-v08-docs)
- [x] PRD §2.1: add requirement for pilot voltage in uplink
- [x] PRD §3.2.1: update payload format (12 bytes with pilot mV at bytes 3-4)
- [x] PRD: fix stale "8-byte" references
- [x] TDD §3.1: update v0x08 layout, encoding example, rationale
- [x] TDD §3.2: fix flags byte offset (byte 7)
- [x] TDD §3.3: update legacy format comparison table

### Firmware — heat call removal
**platform_api_impl.c** (platform layer GPIO driver):
- [ ] Remove `HEAT_CALL_NODE` / `heat_call_gpio` definition
- [ ] Remove heat call GPIO initialization in `platform_gpio_init()`
- [ ] Remove heat call reading in `platform_gpio_get()` case for pin 1

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

**app_tx.c**:
- [ ] Bump version byte to 0x08
- [ ] Payload byte layout unchanged (pilot voltage stays at bytes 3-4)

### AWS
- [ ] `decode_evse_lambda.py`: add v0x08 decoder (same byte offsets as v0x07, no heat flag)
- [ ] v0x07 decoder remains for backward compatibility
- [ ] Remove `thermostat_heat` from v0x08 decoded output
- [ ] Remove `thermostat_heat_active` from DynamoDB item for v0x08
- [ ] `terraform apply` to deploy updated Lambda

### Tests — C (firmware)
- [ ] `test_thermostat_inputs.c`: remove `test_heat_call_high`, `test_heat_call_low`, `test_flags_heat_only`
- [ ] `test_app.c`: remove/update all `mock_gpio_values[1]` heat setup lines; remove `test_thermostat_heat_only`, `test_selftest_boot_gpio_heat_fail`
- [ ] `test_selftest_trigger.c`: remove heat GPIO setup, update pass/fail count expectations
- [ ] `test_app_tx.c`: remove heat from mock GPIO setup, update flag assertions (0x07 → 0x06, etc.), bump version to 0x08
- [ ] `test_shell_commands.c`: remove `test_hvac_status_heat_on`, update heat-related assertions

### Tests — Python (AWS)
- [ ] `test_decode_evse.py`: add v0x08 decode tests (same layout, no heat flag)
- [ ] Remove `thermostat_heat` assertions from v0x08 tests
- [ ] Verify v0x07 backward compat still passes (heat present in old payloads)
- [ ] All tests pass

## Testing Requirements
- [ ] `make -C rak-sid/app/rak4631_evse_monitor/tests/ clean test` — all pass
- [ ] `python3 -m pytest rak-sid/aws/tests/ -v` — all pass
- [ ] On-device: `app evse status` still shows pilot voltage locally
- [ ] On-device: `app hvac status` shows only cool call (no heat)
- [ ] On-device: uplink verified as 12 bytes via `decode_evse_lambda.py` logs

## Deliverables
- Updated firmware: v0x08 payload (12 bytes, pilot voltage retained, heat call removed)
- Updated Lambda: v0x08 + v0x07 backward compat
- Updated tests: C and Python (heat tests removed, v0x08 tests added)
- Terraform deployed
