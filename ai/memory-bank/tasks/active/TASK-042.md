# TASK-042: Privacy agent — find/assign privacy/legal agent, draft policy, CCPA review

**Status**: not started
**Priority**: P2
**Owner**: Pam
**Branch**: `feature/privacy-agent`
**Size**: S (2 points)

## Description
Per PRD 6.4.2, no one currently owns privacy policy, CCPA compliance, data retention rules, or customer data deletion procedures. This task identifies and assigns a dedicated privacy/legal agent, drafts the initial privacy policy using TASK-038's framework, conducts a CCPA compliance review, and establishes ongoing privacy governance.

## Dependencies
**Blocked by**: TASK-038 (needs the draft privacy policy, retention rules, and CCPA checklist)
**Blocks**: none

## Acceptance Criteria
- [ ] Privacy/legal agent identified and assigned (name, role, contact)
- [ ] Agent has reviewed the data inventory (what PII/behavioral data SideCharge collects)
- [ ] Initial privacy policy reviewed and approved by agent
- [ ] CCPA compliance gaps identified with remediation plan
- [ ] Data deletion request procedure defined (who handles, SLA, what gets deleted)
- [ ] Privacy governance documented: who reviews changes, escalation path, review cadence

## Testing Requirements
- [ ] Privacy policy reviewed by legal/privacy agent
- [ ] CCPA checklist walkthrough with agent

## Deliverables
- `docs/privacy-governance.md`
- Updated `docs/privacy-policy.md`
