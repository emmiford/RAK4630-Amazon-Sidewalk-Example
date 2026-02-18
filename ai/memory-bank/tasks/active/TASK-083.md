# TASK-083: Automate data export for Right to Know requests

**Status**: not started
**Priority**: P3
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description
Automate the CCPA "Right to Know" response: given an owner_email, export all personal information and telemetry associated with that customer's device(s) in a machine-readable format (JSON or CSV). Currently this would require manual DynamoDB queries. Automation becomes necessary at 50+ customers to meet the 45-day response SLA. See `docs/privacy-governance.md` §4.1.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] CLI tool or Lambda that accepts owner_email and produces a data export
- [ ] Export includes: all PII fields from device registry, all telemetry events, all daily aggregates
- [ ] Output format: JSON (machine-readable, CCPA compliant)
- [ ] PII fields clearly labeled in export
- [ ] Tool logs the export request (audit trail) without logging the exported data
- [ ] Export can be triggered by operator (not self-service — identity verification still manual)

## Testing Requirements
- [ ] Unit test: mock export flow with sample data
- [ ] Verify export includes all expected data categories

## Deliverables
- `aws/data_export.py` (CLI tool or Lambda)
- `aws/tests/test_data_export.py`
