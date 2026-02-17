# TASK-073: Automate remote diagnostic queries from health digest

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
The remote diagnostics request (0x40 downlink, TASK-029 Tier 2) is initially manual — an operator triggers it via CLI. This task adds automated triggering: the daily health digest Lambda sends a 0x40 diagnostic request to any device that looks unhealthy (offline too long, has recent faults, or has a stale firmware version). Results are included in the next health digest report.

## Dependencies
**Blocked by**: TASK-029 (both Tier 1 health digest and Tier 2 remote diagnostics must exist)
**Blocks**: none

## Acceptance Criteria
- [ ] Health digest Lambda identifies unhealthy devices (offline >2x heartbeat, recent faults, stale firmware)
- [ ] Lambda sends 0x40 downlink to each unhealthy device
- [ ] Next digest includes diagnostics responses received since last digest
- [ ] Configurable: auto-query can be enabled/disabled via environment variable

## Testing Requirements
- [ ] Python tests for unhealthy device detection logic
- [ ] Python tests for 0x40 send integration (mocked)
- [ ] Python tests for diagnostics response correlation

## Deliverables
- `aws/health_digest_lambda.py`: Auto-query logic
- `aws/tests/test_health_digest.py`: Tests for auto-query
