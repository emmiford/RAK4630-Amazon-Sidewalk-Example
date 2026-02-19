# TASK-088: Scheduler integration tests — verify signed payload format

**Status**: not started
**Priority**: P2
**Owner**: Eero
**Branch**: —
**Size**: S (1 point)

## Description
TASK-032 added command signing to `send_charge_command()` and `send_delay_window()` in the charge scheduler Lambda. However, the existing scheduler tests don't set `CMD_AUTH_KEY`, so the signing path is never exercised. Need tests that set the env var and verify:

1. `send_sidewalk_msg()` is called with payload + 8-byte HMAC tag appended
2. Total payload size stays within 19-byte LoRa MTU
3. The HMAC tag matches the expected value for the given key + payload

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Tests for `send_charge_command()` with `CMD_AUTH_KEY` set → payload is 12 bytes (4 + 8 tag)
- [ ] Tests for `send_delay_window()` with `CMD_AUTH_KEY` set → payload is 18 bytes (10 + 8 tag)
- [ ] Tests verify the appended tag matches `cmd_auth.sign_command()` output
- [ ] Tests verify backward compat: no `CMD_AUTH_KEY` → payload unchanged (4 or 10 bytes)

## Testing Requirements
- [ ] Add to `aws/tests/test_charge_scheduler.py`
- [ ] All existing scheduler tests still pass

## Deliverables
- `aws/tests/test_charge_scheduler.py`: ~4 new tests in a `TestCommandSigning` class
