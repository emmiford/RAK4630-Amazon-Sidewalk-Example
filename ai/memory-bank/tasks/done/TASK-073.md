# TASK-073: Automate remote diagnostic queries from health digest

**Status**: committed (2026-02-17, Eliel)
**Priority**: P2
**Owner**: Eliel
**Branch**: task/073-auto-diag-queries
**Size**: M (3 points)

## Description
The remote diagnostics request (0x40 downlink, TASK-029 Tier 2) is initially manual — an operator triggers it via CLI. This task adds automated triggering: the daily health digest Lambda sends a 0x40 diagnostic request to any device that looks unhealthy (offline too long, has recent faults, or has a stale firmware version). Results are included in the next health digest report.

## Dependencies
**Blocked by**: none (TASK-029 merged)
**Blocks**: none

## Acceptance Criteria
- [x] Health digest Lambda identifies unhealthy devices (offline >2x heartbeat, recent faults, stale firmware)
- [x] Lambda sends 0x40 downlink to each unhealthy device
- [x] Next digest includes diagnostics responses received since last digest
- [x] Configurable: auto-query can be enabled/disabled via environment variable

## Testing Requirements
- [x] Python tests for unhealthy device detection logic (7 tests in TestIdentifyUnhealthyReasons)
- [x] Python tests for 0x40 send integration (5 tests in TestSendDiagnosticRequests)
- [x] Python tests for diagnostics response correlation (3 tests in TestGetRecentDiagnostics)

## Deliverables
- `aws/health_digest_lambda.py`: Auto-query logic (identify_unhealthy_reasons, send_diagnostic_requests, get_recent_diagnostics, DIAGNOSTICS RESPONSES digest section)
- `aws/sidewalk_utils.py`: Added wireless_device_id parameter to send_sidewalk_msg
- `aws/tests/test_health_digest.py`: 43 tests (26 new)

## Implementation Notes
- `AUTO_DIAG_ENABLED` env var gates the feature (default "false")
- `LATEST_APP_VERSION` env var sets the target firmware version (default 0 = skip stale check)
- Diagnostics responses are always collected from the last 24h, regardless of auto-diag setting
- `send_sidewalk_msg` now accepts optional `wireless_device_id` for targeted sends
