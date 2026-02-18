# TASK-081: Develop formal incident response plan

**Status**: not started
**Priority**: P3
**Owner**: Pam
**Branch**: —
**Size**: M (3 points)

## Description
Develop a formal data breach incident response plan to replace the interim procedure in `docs/privacy-governance.md` §5.2. Must cover detection, containment, assessment, notification (72-hour CCPA/CPA deadlines), remediation, and post-incident documentation. Should be developed with input from external privacy consultant (TASK-076). Required before multi-customer rollout.

## Dependencies
**Blocked by**: TASK-076 (needs consultant input for legal notification requirements)
**Blocks**: none

## Acceptance Criteria
- [ ] Incident response plan document created
- [ ] Covers all phases: detect, contain, assess, notify, remediate, document
- [ ] State-specific notification deadlines documented (CA 72h, CO 30d, VA/CT "reasonable")
- [ ] Roles and responsibilities assigned (who does what in each phase)
- [ ] Contact list maintained (legal counsel, AWS support, affected customers)
- [ ] Plan reviewed by external privacy consultant
- [ ] Tabletop exercise conducted

## Deliverables
- `docs/incident-response-plan.md`
- Updated `docs/privacy-governance.md` §5 (replace interim with reference to formal plan)
