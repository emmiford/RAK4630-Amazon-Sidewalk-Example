# TASK-038: Data privacy — privacy policy, retention rules, and CCPA compliance review

**Status**: not started
**Priority**: P2
**Owner**: Pam
**Branch**: `feature/data-privacy`
**Size**: M (3 points)

## Description
Per PRD 6.4.2, SideCharge stores behavioral telemetry (AC/EV patterns revealing occupancy and routines) and PII (owner name, email, address, meter number) indefinitely with no retention policy, no privacy policy document, and no CCPA compliance review. This task defines data retention rules, drafts a customer-facing privacy policy, verifies no PII leaks into CloudWatch logs, and defines a customer data deletion procedure.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-042 (privacy agent reviews/finalizes the drafts from this task)

## Acceptance Criteria
- [ ] Data retention policy defined: raw telemetry TTL (e.g., 90 days), aggregated statistics retention period
- [ ] DynamoDB TTL configured per retention policy via Terraform
- [ ] Customer-facing privacy policy drafted
- [ ] CloudWatch logs verified: no PII in log output
- [ ] Customer data deletion procedure defined (what to delete on device return/decommission)
- [ ] CCPA compliance checklist completed (right to know, right to delete, right to opt-out)

## Testing Requirements
- [ ] Verify DynamoDB TTL expiration works as configured
- [ ] Grep CloudWatch logs for PII patterns
- [ ] Walkthrough of data deletion procedure

## Deliverables
- `docs/privacy-policy.md`
- `docs/data-retention.md`
- `aws/terraform/`: Updated TTL settings
- CCPA compliance checklist
