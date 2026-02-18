# TASK-085: E2E Charge Now cloud opt-out verification

**Status**: not started
**Priority**: P2
**Owner**: Eero
**Branch**: —
**Size**: S (2 points)

## Description
End-to-end verification that the Charge Now cloud opt-out protocol (TASK-064 /
ADR-003) works across the full device → cloud → device loop. This covers the
gap between device-side unit tests and cloud-side unit tests — neither proves
that the LoRa uplink flag actually triggers scheduler suppression in production.

Requires physical button wired to P0.07 (see TASK-072 hardware blocker).

## Dependencies
**Blocked by**: TASK-072 (button hardware not yet assembled)
**Blocks**: none

## Procedure
1. Flash device with latest firmware (includes TASK-048b Charge Now latch)
2. Press Charge Now button on device
3. Verify device uplink shows `FLAG_CHARGE_NOW=1` (check DynamoDB:
   `charge_now: true` in `data.evse`)
4. Verify scheduler sentinel (`timestamp=0`) shows `charge_now_override_until`
   written by decode Lambda — value should be 9 PM MT if during TOU peak,
   or ~4 hours from now otherwise
5. Wait for next scheduler invocation (or manually trigger via CLI)
6. Verify scheduler logs "Charge Now opt-out active" and does NOT send a
   delay window downlink
7. After override expires (or simulate by manually clearing the sentinel field):
   - Trigger scheduler again
   - Verify it sends a delay window normally

## Acceptance Criteria
- [ ] Device uplink contains `charge_now=true` after button press
- [ ] Decode Lambda writes `charge_now_override_until` to sentinel
- [ ] Scheduler suppresses pause downlink while override is active
- [ ] Scheduler resumes normal scheduling after override expires
- [ ] CloudWatch logs show "Charge Now opt-out active" message

## Testing Requirements
- [ ] Manual: full loop on physical device with LoRa connectivity
- [ ] DynamoDB query confirms sentinel field values
- [ ] CloudWatch log inspection for scheduler suppression message

## Deliverables
- Verification log in `tests/e2e/`
- Screenshot or DynamoDB query output showing sentinel with override field
