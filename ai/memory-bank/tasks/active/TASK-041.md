# TASK-041: Commissioning checklist card — printed card with 12-step checklist and wiring diagram

**Status**: not started
**Priority**: P0
**Owner**: Bobby
**Branch**: `feature/commissioning-card`
**Size**: M (3 points)

## Description
Per PRD 2.5.2, the commissioning checklist card is the only defense against the most dangerous class of installation errors — 240V branch circuit wiring mistakes that the device cannot detect. P0 for first field install. This task designs a printed card: one side has the 12-step checklist (C-01 through C-12) with pass/fail checkboxes, the other side has a wiring diagram showing current clamp orientation, thermostat terminal mapping (R, Y, C, G), and J1772 pilot tap point.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] Card design: front side with 12-step commissioning checklist (C-01 through C-12)
- [ ] Card design: back side with wiring diagram (current clamp, thermostat terminals, J1772 pilot tap)
- [ ] Card fields: installer name, date, device ID, 12 pass/fail checkboxes, installer signature
- [ ] Pass criteria for each step clearly printed on card (from PRD 2.5.2 table)
- [ ] Card size suitable for attachment to device enclosure or junction box cover
- [ ] Print-ready file format (PDF or similar)

## Testing Requirements
- [ ] Review by electrician or installer for clarity and completeness
- [ ] Verify all 12 steps match PRD 2.5.2 table exactly

## Deliverables
- `docs/commissioning-card.pdf`
- `docs/commissioning-card-source/` (source files)
