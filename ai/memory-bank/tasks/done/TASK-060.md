# TASK-060: Uplink payload v0x08 — reserve heat flag, keep pilot voltage

**Status**: merged done (2026-02-17, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: task/060-uplink-v08 (merged to main, worktree removed)
**Size**: M (5 points)

## Summary

Updated uplink payload from v0x07 to v0x08. Removed thermostat heat call GPIO entirely from firmware (platform layer, app layer, selftest, shell). Bumped version byte 0x07 → 0x08. Payload stays 12 bytes with pilot voltage in bytes 3-4. Lambda decoder handles both v0x07 (with heat) and v0x08 (without heat).

16 files changed (+171 / -183). 71/71 C tests pass, 195/195 Python tests pass.

## Remaining

- `terraform apply` to deploy updated Lambda
- On-device verification after next flash
