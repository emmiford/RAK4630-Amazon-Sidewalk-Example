# TASK-072: On-device Charge Now button GPIO verification

**Status**: not started
**Priority**: P2
**Owner**: —
**Branch**: —
**Size**: XS (1 point)

## Description
TASK-062 wired up the Charge Now button GPIO (P0.07) in the devicetree overlay,
platform_api_impl.c, and TDD. This task covers the on-device manual verification
that the physical button reads correctly and the 5-press selftest trigger fires.

## Dependencies
**Blocked by**: TASK-062 (merged)
**Blocks**: none

## Acceptance Criteria
- [ ] `app selftest` shell command shows button GPIO reads 0 when not pressed, 1 when pressed
- [ ] 5 presses within 5 seconds triggers selftest and LED blink codes display
- [ ] Button wired between P0.07 (WB_IO2) and VDD on the RAK5005-O baseboard

## Testing Requirements
- [ ] Manual: press button, observe shell output
- [ ] Manual: 5-press trigger, observe LED blink pattern

## Deliverables
- Verification log (pass/fail noted in this task file)
