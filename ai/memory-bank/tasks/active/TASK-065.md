# TASK-065: TDD §6.5 self-test rewrite + PRD fault lifecycle

**Status**: in progress (2026-02-16, Pam)
**Priority**: P1
**Owner**: Pam
**Branch**: task/065-selftest-tdd-rewrite
**Size**: S (1 point)

## Description
Rewrite TDD §6.5 Self-Test to document three previously missing items:
1. The production button-triggered self-test (selftest_trigger.c) — existed in code since TASK-040 but was absent from the TDD
2. LED blink code feedback for installers (green=pass count, red=fail count)
3. FAULT_SELFTEST lifecycle and production recovery paths

Also fix FAULT_INTERLOCK description in §3.2 (was vague/wrong — said "charge_allowed=true" but code checks charge_allowed=false with current flowing).

Add FAULT_SELFTEST lifecycle paragraph to PRD §2.5.3 specifying that button re-test clears the flag on all-pass.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-066 (code change to clear fault on button re-test)

## Acceptance Criteria
- [x] TDD §6.5 split into 4 subsections: Boot (6.5.1), Button Trigger (6.5.2), Continuous (6.5.3), Lifecycle (6.5.4)
- [x] TDD §3.2 FAULT_INTERLOCK description matches code behavior
- [x] TDD §3.2 FAULT_SELFTEST references §6.5.4
- [x] PRD §2.5.3 has FAULT_SELFTEST lifecycle paragraph
- [x] PRD §2.5.3 status table includes TASK-066 and TASK-067
- [x] PRD §2.5.1.2 references TASK-067
- [ ] INDEX.md updated with TASK-065, TASK-066, TASK-067

## Deliverables
- docs/technical-design.md (§6.5 rewrite, §3.2 fix)
- docs/PRD.md (§2.5.3 lifecycle, §2.5.1.2 cross-ref)
- ai/memory-bank/tasks/active/TASK-065.md
- ai/memory-bank/tasks/active/TASK-066.md
- ai/memory-bank/tasks/active/TASK-067.md
