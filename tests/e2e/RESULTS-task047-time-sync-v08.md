# TIME_SYNC / Event Buffer / Uplink v0x08 E2E Results — TASK-047

**Date**: 2026-02-17
**Firmware**: main @ e37e1e8 (platform + app rebuilt)
**Tester**: Eero (automated)
**Branch**: task/047-058-device-verification

## Context

Combined device verification for:
- TASK-033: TIME_SYNC downlink handling
- TASK-034: Event buffer with ACK watermark trimming
- TASK-035: Uplink v0x07 → now v0x08 (12-byte payload with timestamp + charge_allowed)

Note: Task title says v0x07 but firmware is at v0x08 (heat bit dropped, cool-only thermostat).

## Uplink Payload Verification

### Raw payload (hex): `e50804070144096000000000`

| Byte | Hex | Field | Decoded | Match |
|------|-----|-------|---------|-------|
| 0 | E5 | magic | 0xE5 | PASS |
| 1 | 08 | version | 8 | PASS |
| 2 | 04 | j1772_state | D (Error) | PASS (matches serial) |
| 3-4 | 07 01 | pilot_mv | 263 mV (LE) | PASS (matches serial) |
| 5-6 | 44 09 | current_ma | 2372 mA (LE) | PASS (matches serial) |
| 7 | 60 | flags | 0x60 = CLAMP+INTERLOCK | PASS |
| 8-11 | 00 00 00 00 | timestamp | 0 (not synced) | PASS |

**Payload size**: 12 bytes — confirmed.

### DynamoDB Event (schema 2.1)

Timestamp: 2026-02-18T02:26:28.203Z, RSSI: -87 dBm, Link: LoRa

| Field | Value | Expected | Match |
|-------|-------|----------|-------|
| version | 8 | 8 (v0x08) | PASS |
| pilot_state | "D" | "D" | PASS |
| pilot_state_code | 4 | 4 | PASS |
| pilot_voltage_mv | 263 | 263 | PASS |
| current_draw_ma | 2372 | 2372 | PASS |
| charge_allowed | false | false | PASS |
| charge_now | false | false | PASS |
| device_timestamp_epoch | 0 | 0 (not synced) | PASS |
| thermostat_cool_active | false | false | PASS |
| thermostat_bits | 0 | 0 | PASS |
| fault_clamp_mismatch | true | true (bench) | PASS |
| fault_interlock | true | true (bench) | PASS |
| fault_selftest_fail | false | false | PASS |
| fault_sensor | false | false | PASS |

### Backward Compatibility

- v0x06 decode: verified by 195 Python unit tests (TestDecodeV07Payload::test_v06_backward_compat)
- v0x07 decode: verified by 195 Python unit tests (TestDecodeV08Payload::test_v07_backward_compat_still_has_heat)

## Shell Commands

| Command | Result | Output |
|---------|--------|--------|
| `app sid time` | PASS | "NOT SYNCED (no TIME_SYNC received)" — correct, no downlink received |
| `app evse buffer` | PASS | "50/50 entries, Oldest: 0, Newest: 0" — buffer full, no watermark trim |

## Blocked Items (PSA -149)

The following acceptance criteria require a working TIME_SYNC downlink, which is blocked by PSA Error -149 (HUK invalidated by platform reflash without MFG — KI-002):

| Criteria | Status | Reason |
|----------|--------|--------|
| TIME_SYNC: `sid time` shows synced time | BLOCKED | No downlink decrypt |
| TIME_SYNC: Clock drift < 10s/day | BLOCKED | Needs synced time first |
| Event buffer: ACK watermark trims entries | BLOCKED | Watermark comes via TIME_SYNC |

### Resolution

Reflash MFG credentials before platform to restore HUK-derived PSA keys:
```
flash.sh mfg && flash.sh platform && flash.sh app
```
Then re-run TIME_SYNC tests. Uplink path is fully verified.

## Summary

| Test Area | Pass | Blocked | Total |
|-----------|------|---------|-------|
| Uplink v0x08 format | 7/7 | 0 | 7 |
| DynamoDB decode | 14/14 | 0 | 14 |
| Backward compat | 2/2 | 0 | 2 |
| Shell commands | 2/2 | 0 | 2 |
| TIME_SYNC verification | 0/0 | 3 | 3 |
| **Total** | **25/25** | **3** | **28** |

**Verdict**: PARTIAL PASS — All testable items pass. 3 items blocked by PSA crypto (KI-002), pending MFG reflash.
