# TASK-029: Production observability — CloudWatch alerting and remote status query

**Status**: in progress (2026-02-17, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: `task/029-prod-observability`
**Size**: L (5 points)

## Description
Per PRD 5.3.2 and 5.3.3, production observability is too thin — no offline alerting, no remote query, no interlock state change logging. Tier 1: cloud alerting (device offline detection, OTA failure alerting, interlock state change logging, daily health digest) with alarms initially disabled. Tier 2: new downlink command (0x40) for on-demand status uplink with extended diagnostics (v1.1 deliverable).

## Dependencies
**Blocked by**: none
**Blocks**: none (but required before production deployment)

## Acceptance Criteria
- [ ] CloudWatch metric filter on DynamoDB writes per device_id; alarm when no write for 2x heartbeat interval
- [ ] CloudWatch alarm on OTA sender Lambda errors or stalled sessions (no ACK for >5 min)
- [ ] CloudWatch metric filter for cool_call transitions (interlock state change logging)
- [ ] Scheduled Lambda for daily health digest (last-seen, firmware version, error counts)
- [ ] All Tier 1 alarms deployed but initially disabled (generous thresholds)
- [ ] Tier 2: Remote status request downlink (0x40) triggers immediate extended uplink
- [ ] Tier 2: Extended diagnostics payload fits within 19-byte LoRa MTU

## Testing Requirements
- [ ] Tier 1: Terraform plan validates CloudWatch resources
- [ ] Tier 1: Python tests for daily health digest Lambda
- [ ] Tier 2: C unit tests for 0x40 downlink parsing and status uplink encoding
- [ ] Tier 2: Python tests for decode Lambda handling extended diagnostics payload

## Deliverables
- `aws/terraform/`: CloudWatch alarms, metric filters, SNS topic
- `aws/health_digest_lambda.py`
- `aws/tests/test_health_digest.py`
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Handle 0x40 (Tier 2)
- `aws/decode_evse_lambda.py`: Extended diagnostics (Tier 2)
