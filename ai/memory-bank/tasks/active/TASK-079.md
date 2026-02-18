# TASK-079: Build consumer privacy request intake form

**Status**: not started
**Priority**: P3
**Owner**: Pam
**Branch**: —
**Size**: S (2 points)

## Description
Create a process and intake mechanism for consumer privacy requests (Right to Know, Right to Delete, Right to Correct) as defined in `docs/privacy-governance.md` §4. Initial implementation can be a simple email-based process with spreadsheet tracking; future versions should have a web form with automated SLA tracking. Required for CCPA/CPA compliance when serving non-developer customers.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Privacy request email address established (privacy@sidecharge.com or equivalent)
- [ ] Request tracking spreadsheet/system created with fields: date received, request type, requester identity, verified (Y/N), deadline (45 days), completion date
- [ ] Identity verification procedure documented
- [ ] Response templates drafted for each request type (Know, Delete, Correct)
- [ ] Process documented in `docs/privacy-governance.md` §4.2

## Deliverables
- Privacy request email address
- Request tracking spreadsheet template
- Response templates
- Updated `docs/privacy-governance.md`
