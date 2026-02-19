# TASK-032: Cloud command authentication — signed downlinks for command authenticity

**Status**: in progress (2026-02-19, Eliel)
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/032-cloud-cmd-auth`
**Size**: L (5 points)

## Description
Per PRD 6.3.2, downlinks are encrypted in transit by Sidewalk but not signed — no per-command authenticity verification. A compromised cloud layer could send arbitrary charge control commands. This task adds command-level authentication: cloud signs each downlink payload, device verifies before executing.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] All charge control downlinks (0x10) include an authentication tag
- [x] Device verifies authentication tag before executing any charge control command
- [x] Unsigned or incorrectly signed commands are rejected with error log
- [x] Authentication fits within 19-byte LoRa MTU (truncated HMAC or compact signature)
- [x] Key provisioning procedure documented

## Testing Requirements
- [x] Python tests: command signing in charge scheduler Lambda (12 tests)
- [x] C unit tests: authentication verification pass/fail paths (8 tests)
- [x] C unit tests: reject unsigned commands, accept signed commands (6 RX integration tests)

## Design

**Algorithm**: HMAC-SHA256, truncated to 8 bytes (64-bit tag).

**Wire format**: `[payload bytes] [8-byte HMAC tag]`
- Legacy charge control: 4 + 8 = 12 bytes (fits 19-byte LoRa MTU)
- Delay window: 10 + 8 = 18 bytes (fits 19-byte LoRa MTU)

**Key management**:
- Cloud: `CMD_AUTH_KEY` environment variable (hex-encoded, 32 bytes)
- Device: Compiled constant in `app_entry.c` via `cmd_auth_set_key()`
- Auth is only active when a key is configured; unconfigured = pass-through
- Generate key: `python3 -c "import secrets; print(secrets.token_hex(32))"`

**Security properties**:
- Constant-time tag comparison (timing-safe)
- 2^64 brute-force resistance (adequate for this threat model)
- No replay protection (not in scope; Sidewalk transport provides ordering)

## Deliverables
- `app/rak4631_evse_monitor/include/cmd_auth.h`: Auth interface
- `app/rak4631_evse_monitor/src/app_evse/cmd_auth.c`: SHA-256 + HMAC + verify
- `aws/cmd_auth.py`: Python signing module
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Auth verification in RX path
- `aws/charge_scheduler_lambda.py`: Signed downlinks
- `aws/tests/test_cmd_auth.py`: 12 Python tests
- `app/rak4631_evse_monitor/tests/test_app.c`: 14 C tests (8 HMAC + 6 RX integration)
