# TASK-084: Populate registry app_version from diagnostics responses

**Status**: MERGED DONE (2026-02-19, Eliel)
**Priority**: P2
**Size**: S (1 point)

## Summary
One-line fix in `decode_evse_lambda.py` to also extract `app_version` from diagnostics (0xE6) payloads when updating the device registry. Previously only OTA status uplinks populated this field, leaving USB-flashed devices at version 0. Two new Python tests added (262 total passing).
