# TASK-082: Add geolocation opt-out mechanism

**Status**: not started
**Priority**: P3
**Owner**: Eliel
**Branch**: —
**Size**: S (2 points)

## Description
Under CCPA, `install_lat` and `install_lon` qualify as "precise geolocation" (sensitive personal information). Consumers have the right to limit use of sensitive data. This task adds an opt-out mechanism: either stop collecting lat/lon entirely, or limit stored location to utility region only (sufficient for TOU schedule lookup). Requires legal guidance from privacy consultant (TASK-076) on whether address alone (without lat/lon) triggers the sensitive data provision.

## Dependencies
**Blocked by**: TASK-076 (needs legal guidance on scope)
**Blocks**: none

## Acceptance Criteria
- [ ] Legal guidance obtained on whether install_address alone triggers CCPA sensitive data provision
- [ ] Opt-out mechanism implemented (either: make lat/lon optional in commissioning, or store utility region instead)
- [ ] Device registry updated: lat/lon fields nullable or removed
- [ ] Privacy policy updated to reflect change
- [ ] Existing lat/lon data deletion procedure defined for opted-out customers

## Deliverables
- Updated device registry schema
- Updated `docs/privacy-policy.md`
- Updated commissioning flow (if lat/lon made optional)
