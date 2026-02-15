# TASK-032: Cloud command authentication — signed downlinks for command authenticity

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: `feature/cmd-auth`
**Size**: L (5 points)

## Description
Per PRD 6.3.2, downlinks are encrypted in transit by Sidewalk but not signed — no per-command authenticity verification. A compromised cloud layer could send arbitrary charge control commands. This task adds command-level authentication: cloud signs each downlink payload, device verifies before executing.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] All charge control downlinks (0x10) include an authentication tag
- [ ] Device verifies authentication tag before executing any charge control command
- [ ] Unsigned or incorrectly signed commands are rejected with error log
- [ ] Authentication fits within 19-byte LoRa MTU (truncated HMAC or compact signature)
- [ ] Key provisioning procedure documented

## Testing Requirements
- [ ] Python tests: command signing in charge scheduler Lambda
- [ ] C unit tests: authentication verification pass/fail paths
- [ ] C unit tests: reject unsigned commands, accept signed commands

## Deliverables
- `aws/charge_scheduler_lambda.py`: Command signing
- `aws/sidewalk_utils.py`: Authentication utility functions
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Command auth verification
- `aws/tests/test_cmd_auth.py`
- `tests/app/test_cmd_auth.c`
