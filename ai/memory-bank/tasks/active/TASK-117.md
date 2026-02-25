# TASK-117: Execute EXP-001 — BLE-less LoRa-only Sidewalk feasibility test

**Status**: not started
**Priority**: P2
**Owner**: Oliver
**Branch**: —
**Size**: M (5 points)

## Description
Execute the 4-phase experiment protocol documented in `docs/experiment-log.md` (EXP-001). Determine whether Amazon Sidewalk firmware can build, boot, and operate LoRa without BLE compiled in, and whether session keys are portable between boards.

Requires hands-on hardware testing with RAK4631 boards.

## Dependencies
**Blocked by**: none (protocol written in TASK-115)
**Blocks**: Future module cost-reduction decisions (LoRa-E5, RAK3172 evaluation)

## Acceptance Criteria
- [ ] Phase 1: Compile-time test completed and results recorded
- [ ] Phase 2: Boot test completed (if Phase 1 passes)
- [ ] Phase 3: LoRa connectivity test completed (if Phase 2 passes)
- [ ] Phase 4: Session key portability test completed (if Phase 3 passes)
- [ ] `docs/experiment-log.md` updated with results and verdict
- [ ] ADR-010 updated or superseded based on findings

## Testing Requirements
- [ ] All phases documented with serial console logs and DynamoDB evidence

## Deliverables
- Updated `docs/experiment-log.md` with EXP-001 results + verdict
- ADR update if findings change module selection decision
