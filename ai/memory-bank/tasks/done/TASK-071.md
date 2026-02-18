# TASK-071: Scheduler sentinel divergence detection

**Status**: MERGED DONE (2026-02-17, Eliel)
**Priority**: P2
**Branch**: `task/071-sentinel-divergence`
**Size**: S (2 points)

## Summary

Detects when the scheduler sentinel's `last_command` diverges from the device's
reported `charge_allowed` state (caused by lost LoRa downlinks). The decode Lambda
compares each EVSE uplink against the sentinel, re-invokes the scheduler with
`force_resend` on mismatch, and caps retries at 3 before triggering a CloudWatch alarm.

## Deliverables

- `decode_evse_lambda.py` — `check_scheduler_divergence()` with retry tracker (DynamoDB timestamp=-3)
- `charge_scheduler_lambda.py` — `force_resend` flag bypasses heartbeat dedup
- Terraform — `SCHEDULER_LAMBDA_NAME` env var, IAM cross-invoke, CloudWatch alarm
- 13 new Python tests (221 total)
