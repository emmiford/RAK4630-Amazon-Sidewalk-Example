# TASK-068: TDD §6.5 self-test rewrite + PRD fault lifecycle

**Status**: in progress
**Priority**: P1
**Owner**: Pam
**Branch**: task/065-selftest-tdd-prd (legacy — created before renumber)
**Size**: S (2 points)

## Description

Rewrite TDD §6.5 (self-test and fault detection) to accurately document the implemented
behavior from TASK-039 and TASK-040. Update PRD §2.5.3 fault flag lifecycle to match the
actual implementation (FAULT_SELFTEST is latched until reboot or button re-test per
TASK-066).

**Note**: This task was originally tracked as TASK-065 by Pam, but that ID was already
used for Eliel's AC-priority interlock (merged). Renumbered to TASK-068 to resolve the
ID collision. The worktree branch `task/065-selftest-tdd-prd` predates the renumber.

## Dependencies

**Blocked by**: none
**Blocks**: TASK-066 (button re-test needs accurate fault lifecycle spec)

## Acceptance Criteria

- [ ] TDD §6.5 accurately describes boot self-test, continuous monitoring, and on-demand trigger
- [ ] TDD §6.5.4 documents FAULT_SELFTEST lifecycle (latched on failure, cleared on reboot or button re-test)
- [ ] PRD §2.5.3 fault flag descriptions match TDD
- [ ] No contradictions between PRD and TDD on self-test behavior

## Deliverables

- Updated `docs/technical-design.md` — §6.5 rewrite
- Updated `docs/PRD.md` — §2.5.3 fault lifecycle corrections
