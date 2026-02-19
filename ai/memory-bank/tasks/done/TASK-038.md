# TASK-038: Data privacy — privacy policy, retention rules, and CCPA compliance review

**Status**: MERGED DONE (2026-02-17, Pam)
**Priority**: P2
**Owner**: Pam
**Branch**: `task/038-data-privacy`
**Size**: M (3 points)

## Description
Per PRD 6.4.2, SideCharge stores behavioral telemetry (AC/EV patterns revealing occupancy and routines) and PII (owner name, email, address, meter number) indefinitely with no retention policy, no privacy policy document, and no CCPA compliance review. This task defines data retention rules, drafts a customer-facing privacy policy, verifies no PII leaks into CloudWatch logs, and defines a customer data deletion procedure.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-042 (privacy agent reviews/finalizes the drafts from this task)

## Acceptance Criteria
- [x] Data retention policy defined: raw telemetry TTL (90 days), aggregated statistics retention (3 years), PII deletion on decommission
- [x] DynamoDB TTL configured per retention policy via Terraform (events table TTL enabled; decode Lambda sets `ttl` attribute on every item)
- [x] Customer-facing privacy policy drafted (`docs/privacy-policy.md`)
- [x] CloudWatch logs verified: no PII in log output (audit checklist in `docs/data-retention.md` §4)
- [x] Customer data deletion procedure defined (`docs/data-retention.md` §2)
- [x] CCPA compliance checklist completed (`docs/data-retention.md` §3)

## Testing Requirements
- [x] Verify DynamoDB TTL: decode Lambda sets `ttl` = timestamp + 90 days on every item (unit test added)
- [x] CloudWatch PII audit: code review of all 4 Lambdas — no Tier 1 fields logged
- [x] Walkthrough of data deletion procedure (defined in data-retention.md §2)

## Deliverables
- [x] `docs/privacy-policy.md` — customer-facing privacy policy (DRAFT, needs legal review)
- [x] `docs/data-retention.md` — internal retention rules, deletion procedures, CCPA checklist, PII audit
- [x] `aws/terraform/main.tf` — CloudWatch log retention updated 14→30 days (all 4 log groups)
- [x] `aws/decode_evse_lambda.py` — TTL attribute added to DynamoDB items (90-day expiry)
- [x] `aws/tests/test_decode_evse.py` — TTL unit test added

## Changes Made (2026-02-17)
1. **decode Lambda**: Added `ttl` attribute to every DynamoDB item (= `floor(timestamp_ms/1000) + 7776000` = 90 days). Previously TTL was enabled on the table but Lambda never set the attribute, so items never expired.
2. **Terraform**: CloudWatch log retention updated from 14 days to 30 days on all 4 Lambda log groups (evse-decoder, charge-scheduler, ota-sender, health-digest) to match data-retention.md policy.
3. **data-retention.md**: Added health-digest log group to §1.4, updated PII audit in §4 (confirmed OK for all 4 Lambdas), updated implementation status §5.
4. **Test**: Added `test_ttl_attribute_set_for_90_day_retention` to verify TTL is set correctly.
