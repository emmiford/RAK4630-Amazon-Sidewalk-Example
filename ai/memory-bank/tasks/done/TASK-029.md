# TASK-029: Production observability — CloudWatch alerting and remote status query

**Status**: MERGED DONE (2026-02-17, Eliel)
**Priority**: P1
**Branch**: `task/029-prod-observability` (merged to main)

## Summary
Tier 1: Health digest Lambda (daily fleet summary via SNS), CloudWatch metric
filters (device_uplink, interlock_activation), CloudWatch alarms (device offline),
SNS alerting topic, Terraform for all resources. Alarms initially disabled.

Tier 2: 0x40 downlink triggers 14-byte 0xE6 diagnostic response (app version,
uptime, boot count, error code, state flags, event buffer pending). Firmware
module diag_request.c/h, decode_evse_lambda.py decoder, full test coverage.

Tests: 73/73 C (18 new), 180/180 Python (31 new).
Spawned: TASK-073 (automate diagnostics, P2), TASK-074 (device registry GSI, P3).
