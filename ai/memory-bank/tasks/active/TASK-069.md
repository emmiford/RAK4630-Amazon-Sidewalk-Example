# TASK-069: Interlock transition event logging

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: M (3 points)

## Description

PRD §2.0.6 requires interlock transition events logged to cloud: AC→EV, EV→AC, override
start/end, with timestamps and reasons. Currently the uplink payload includes interlock
state as a snapshot, but no transition events with timing/duration/reason are sent. This
data is essential for demand response reporting, debugging, and demonstrating code
compliance to inspectors.

### What needs to happen

1. **Device side**: On each interlock state transition, write a transition event to the
   event buffer (TASK-034, already implemented) with: previous state, new state, reason
   (AC priority, cloud override, Charge Now, timeout), timestamp.
2. **Cloud side**: Decode Lambda extracts transition events and writes them to a
   DynamoDB transitions table (or appends to the device record).
3. **Reporting**: Transition history queryable for demand response compliance
   (how many hours was charging delayed, why, when did it resume).

## Dependencies

**Blocked by**: none (event buffer exists, timestamps exist)
**Blocks**: none

## Acceptance Criteria

- [ ] Interlock state transitions (AC→EV, EV→AC) generate event buffer entries
- [ ] Each event includes: previous state, new state, reason enum, timestamp
- [ ] Cloud decode Lambda parses transition events and stores them
- [ ] Transition history queryable per device (DynamoDB or CloudWatch)

## Testing Requirements

- [ ] C unit test: transition from allow→pause generates event with correct reason
- [ ] C unit test: transition from pause→allow generates event
- [ ] Python test: decode Lambda parses transition event correctly
- [ ] Python test: transition stored in DynamoDB

## Deliverables

- Modified `charge_control.c` — emit transition events to event buffer
- New transition event wire format (fits in existing uplink structure)
- Modified `decode_evse_lambda.py` — parse and store transitions
- Modified Terraform — DynamoDB schema update if needed
