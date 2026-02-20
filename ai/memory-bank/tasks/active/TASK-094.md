# TASK-094: Merge PRD + TDD inline wiring doc updates

**Status**: merged done (2026-02-19)
**Priority**: P1
**Owner**: Pam + Utz
**Branch**: `task/prd-hvac-return-paths` (PRD), `task/tdd-hvac-return-paths` (TDD)
**Size**: S (1 point)

## Description
Review and merge the two documentation branches that update the PRD (v1.6) and TDD to reflect the inline pass-through wiring architecture for thermostat (Y, W) and EVSE pilot signals.

Both branches have committed changes:
- **PRD** (`task/prd-hvac-return-paths`): 96 insertions, 75 deletions across 20+ sections. Terminal count 6→11, relay count 2→3, heat call (W-in/W-out) added, PILOT split to PILOT-in/PILOT-out.
- **TDD** (`task/tdd-hvac-return-paths`): 147 insertions, 28 deletions across 14 sections. New §9.3 (inline signal paths + hardware interlock), flags byte bit 0 = HEAT, boot sequence updated, pin mapping updated.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-095, TASK-096, TASK-097, TASK-098, TASK-099

## Acceptance Criteria
- [ ] PRD branch reviewed and merged to main
- [ ] TDD branch reviewed and merged to main
- [ ] No merge conflicts
- [ ] All tests pass after merge
- [ ] Worktrees cleaned up

## Testing Requirements
- [ ] C unit tests pass (`cmake ... && ctest`)
- [ ] Python tests pass (`pytest aws/tests/ -v`)

## Deliverables
- Updated `docs/PRD.md` (v1.6) on main
- Updated `docs/technical-design.md` on main
