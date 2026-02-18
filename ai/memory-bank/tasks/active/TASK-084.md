# TASK-084: Populate registry app_version from diagnostics responses

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
The device registry `app_version` field only gets updated from OTA status uplinks (`decode_evse_lambda.py:628`). For USB-flashed devices that have never done an OTA cycle, it stays at 0. Diagnostics responses (0xE6, `payload_type == 'diagnostics'`) already carry `app_version` but the decode Lambda doesn't feed it to the registry. Fix the decode Lambda to also extract `app_version` from diagnostics payloads.

This makes the health digest version distribution accurate for USB-flashed devices and prevents false stale-firmware flags from TASK-073's auto-diagnostics.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] `decode_evse_lambda.py` updates device registry `app_version` from diagnostics responses
- [ ] Existing OTA version update path unchanged
- [ ] Registry correctly reflects device version after a single 0x40 query

## Testing Requirements
- [ ] Python test: diagnostics response updates registry app_version
- [ ] Python test: EVSE telemetry still passes None (no regression)

## Deliverables
- `aws/decode_evse_lambda.py`: Extract app_version from diagnostics payloads
- `aws/tests/test_decode_evse.py`: Tests for registry update from diagnostics
