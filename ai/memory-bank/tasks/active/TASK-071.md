# TASK-071: Scheduler sentinel divergence detection

**Status**: in progress (2026-02-17, Eliel)
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/071-sentinel-divergence`
**Size**: S (2 points)

## Description

TDD §8.4 identifies a known gap: if a LoRa downlink is lost (pause or allow command),
the cloud's scheduler sentinel thinks the device received the command but the device
never did. The sentinel's `last_command` diverges from the device's actual
`charge_allowed` state reported in uplinks.

### Fix

In the decode Lambda, after writing the uplink to DynamoDB, compare the device's
reported `charge_allowed` bit against the sentinel's `last_command`. On mismatch,
re-trigger the scheduler to re-send the command. Add a re-send counter to prevent
infinite loops (max 3 retries, then alert).

## Dependencies

**Blocked by**: none
**Blocks**: none

## Acceptance Criteria

- [ ] Decode Lambda compares uplink `charge_allowed` against sentinel `last_command`
- [ ] On mismatch, scheduler is re-invoked for that device
- [ ] Re-send counter prevents infinite retry loops (max 3)
- [ ] After 3 retries with continued divergence, CloudWatch alarm fires
- [ ] Normal case (no divergence) adds zero latency to decode path

## Testing Requirements

- [ ] Python test: uplink with charge_allowed=true, sentinel last_command=pause → re-trigger
- [ ] Python test: uplink with charge_allowed=false, sentinel last_command=pause → no action
- [ ] Python test: retry counter reaches 3 → alarm, no more retries

## Deliverables

- Modified `decode_evse_lambda.py` — divergence check after uplink write
- Modified `charge_scheduler_lambda.py` — support re-triggered invocation
- Modified Terraform — CloudWatch alarm for divergence
