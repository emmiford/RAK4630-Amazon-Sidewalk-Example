# TASK-032: Cloud command authentication (HMAC-SHA256)

**Status**: MERGED DONE
**Priority**: P2
**Owner**: Eliel
**Completed**: 2026-02-19
**Branch**: `task/032-cloud-cmd-auth`

## Summary
Added HMAC-SHA256 command authentication for charge control downlinks (0x10). Cloud signs each payload with a truncated 8-byte tag; device verifies before executing. Auth is opt-in — only active when a key is provisioned via `cmd_auth_set_key()`. Standalone SHA-256 implementation (no external crypto deps). Fits within 19-byte LoRa MTU for both legacy (12B) and delay window (18B) commands.

## Deliverables
- `cmd_auth.h` / `cmd_auth.c`: SHA-256 + HMAC + verify (device)
- `cmd_auth.py`: Python signing module (cloud)
- `app_rx.c`: Auth verification in RX path
- `charge_scheduler_lambda.py`: Signed downlinks
- 14 C tests + 12 Python tests

## Follow-up Tasks
- TASK-086: Terraform CMD_AUTH_KEY env var
- TASK-087: Generate + provision production auth key
- TASK-088: Scheduler integration tests for signed payloads
