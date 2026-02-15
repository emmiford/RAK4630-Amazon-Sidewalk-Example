# TASK-030: Fleet command throttling — staggered random delays on charge control downlinks

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: `feature/fleet-throttling`
**Size**: M (3 points)

## Description
Per PRD 6.3.2, a compromised cloud could coordinate simultaneous load switching across all devices, creating a massive demand spike on the grid. This task adds fleet-wide command throttling: any cloud command targeting multiple devices is staggered with randomized 0-10 minute delays. Device-side rate limiting: no more than one charge control command per 5 minutes.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Charge scheduler Lambda staggers downlinks with per-device random delay (0-10 min window)
- [ ] Device-side rate limiting: ignore charge control commands arriving faster than 1 per 5 minutes
- [ ] CloudWatch anomaly detection alarm for unusual command patterns
- [ ] Rate limit configurable via Lambda environment variable

## Testing Requirements
- [ ] Python tests: staggered delay distribution within expected window
- [ ] Python tests: anomaly detection threshold logic
- [ ] C unit tests: device-side rate limiting rejects rapid commands, accepts after cooldown

## Deliverables
- `aws/charge_scheduler_lambda.py`: Staggered delay logic
- `aws/terraform/`: CloudWatch anomaly detection alarm
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Local rate limiting
- `tests/app/test_rate_limit.c`
