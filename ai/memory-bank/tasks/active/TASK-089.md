# TASK-089: Update technical-design.md for v0x09 payload + event buffer drain

**Status**: not started
**Priority**: P3
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
TASK-069 bumped the uplink payload from v0x08 (12 bytes) to v0x09 (13 bytes) with a `transition_reason` byte at index 12. The event buffer drain feature (sends buffered events during idle ticks) was also added. Both need documenting in the technical design doc.

### What needs updating
1. **Section 3.1**: Rename "EVSE Payload v0x08 (Current)" → v0x09, add byte 12 (`transition_reason`), update size from 12 to 13
2. **Format history table** (section 3.3): Add v0x09 row, move v0x08 to legacy
3. **Event buffer section**: Document drain mechanism (cursor, rate-limited sends, trim-aware reset)
4. **decode_evse_lambda.py docstring**: Add v0x09 to the header comment

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] TDD section 3.1 documents v0x09 (13 bytes, transition_reason at byte 12)
- [ ] Format history table updated
- [ ] Event buffer drain behavior documented
- [ ] decode_evse_lambda.py docstring mentions v0x09

## Deliverables
- `docs/technical-design.md` — updated sections 3.1, 3.3, event buffer
- `aws/decode_evse_lambda.py` — updated docstring
