# TASK-042: Privacy agent — find/assign privacy/legal agent, draft policy, CCPA review

**Status**: committed (2026-02-17, Pam)
**Priority**: P2
**Owner**: Pam
**Branch**: `task/038-data-privacy`
**Size**: S (2 points)

## Description
Per PRD 6.4.2, no one currently owns privacy policy, CCPA compliance, data retention rules, or customer data deletion procedures. This task identifies and assigns a dedicated privacy/legal agent, drafts the initial privacy policy using TASK-038's framework, conducts a CCPA compliance review, and establishes ongoing privacy governance.

## Dependencies
**Blocked by**: TASK-038 (needs the draft privacy policy, retention rules, and CCPA checklist — DONE)
**Blocks**: none

## Acceptance Criteria
- [x] Privacy/legal agent identified and assigned — distributed model: Pam (privacy lead), Eliel (technical reviewer), external consultant (TBD for multi-customer)
- [x] Agent has reviewed the data inventory — full PII + behavioral data inventory in `docs/privacy-governance.md` §2
- [x] Initial privacy policy reviewed and approved by agent — Pam reviewed; external legal review required before publication
- [x] CCPA compliance gaps identified with remediation plan — see `docs/data-retention.md` §3 and `docs/privacy-governance.md` §6
- [x] Data deletion request procedure defined — see `docs/privacy-governance.md` §4 (who handles, SLA, what gets deleted)
- [x] Privacy governance documented — review cadence, escalation paths, PR review checklist in `docs/privacy-governance.md` §3

## Testing Requirements
- [x] Privacy policy reviewed by Pam (privacy lead) — external legal review deferred to pre-customer milestone
- [x] CCPA checklist walkthrough — completed in `docs/data-retention.md` §3, gaps tracked in `docs/privacy-governance.md` §6

## Deliverables
- [x] `docs/privacy-governance.md` — privacy roles, data inventory, review process, consumer request handling, incident response, roadmap
- [x] Updated `docs/privacy-policy.md` — cross-reference to governance doc (added)

## Notes
The governance structure is a "distributed ownership" model appropriate for pre-revenue single-device stage. Key escalation: engage external privacy consultant before first non-developer customer deployment. Open items tracked in privacy-governance.md §6.
