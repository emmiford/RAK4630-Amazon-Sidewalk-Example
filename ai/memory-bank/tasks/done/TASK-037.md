# TASK-037: Utility identification — per-device meter number to utility/TOU schedule lookup

**Status**: MERGED DONE (2026-02-17, Pam) — PRD scoping complete; v1.1 implementation deferred
**Priority**: P2
**Owner**: Pam (scoping) / Eliel (v1.1 implementation)
**Branch**: `task/038-data-privacy`
**Size**: S (2 points) — PRD scoping complete; implementation is separate v1.1 task

## Description
Per PRD 4.4, the charge scheduler hardcodes Xcel Colorado (PSCO region, weekdays 5-9 PM MT). To support multiple utilities, each device needs a utility/TOU schedule lookup. Research shows meter numbers are utility-specific with no national standard. Correct pipeline: address → utility → TOU schedule, with meter number used only for rate plan disambiguation.

Product scoping complete in PRD section 4.5. Designed the three-step lookup pipeline, TOU schedule JSON data model with peak window arrays, documented reference schedules for top 5 US residential EV utility markets. Implementation is a v1.1 deliverable (v1.0 remains Xcel-only).

## Dependencies
**Blocked by**: TASK-036 (device registry provides meter_number and install_address — DONE)
**Blocks**: none

## Acceptance Criteria
- [x] Lookup pipeline corrected: address → utility → TOU/WattTime, meter → rate plan
- [x] TOU schedule JSON data model defined
- [x] Reference schedules for top 5 US utility markets documented
- [x] Charge scheduler refactor path defined (4 specific changes)
- [x] Configuration storage phased: v1.0 hardcoded, v1.1 DynamoDB table, v1.1+ OpenEI API
- [x] Open question resolved: meter number vs. address roles clarified
- [ ] Implementation (DynamoDB schedule table, scheduler refactor) — v1.1, NOT STARTED (Eliel)

## Deliverables
- [x] PRD section 4.5
- [x] `docs/design/utility-identification-scope.md`
- [ ] DynamoDB schedule table (v1.1 — Eliel)
- [ ] Scheduler refactor (v1.1 — Eliel)

## Notes
Pam's product scoping work is complete. The remaining v1.1 implementation (DynamoDB TOU schedule table + charge_scheduler refactor) is backend engineering work owned by Eliel, to be tracked as a separate task when v1.1 development begins.
