# TASK-064: Cloud Charge Now protocol — detection, opt-out guard, heartbeat

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: M (5 points)

## Description
Implement the cloud-side Charge Now protocol per ADR-002 and PRD 4.4.5. Three
pieces, tightly coupled:

### 1. Decode Lambda: Charge Now detection
When `decode_evse_lambda` sees `FLAG_CHARGE_NOW=1` (bit 3) in an EVSE uplink:
- Read the scheduler sentinel (`timestamp=0`)
- Compute `charge_now_override_until` = end of current peak window
  (e.g., 9 PM MT for TOU peak). If no peak window is active, set to
  `now + 4 hours` as a conservative default
- Write `charge_now_override_until` to the sentinel

### 2. Scheduler: Charge Now opt-out guard
Before sending a pause command/delay window, the scheduler checks the sentinel:
- If `charge_now_override_until` exists and `now < charge_now_override_until`,
  skip the downlink and log "Charge Now opt-out active"
- After the timestamp passes, resume normal scheduling

### 3. Scheduler: heartbeat re-send
Add a staleness TTL to the dedup logic. Currently the scheduler skips the
downlink if `last_command == command`. Change to: skip if `last_command ==
command AND last_sent < N minutes ago` (default 30 min). This covers lost LoRa
downlinks without fighting Charge Now because the opt-out guard (above) takes
precedence.

### Peak window boundary lookup
The decode Lambda needs to know when the current peak window ends to compute
`charge_now_override_until`. Options:
- **v1.0 (hardcoded)**: Xcel Colorado TOU peak ends at 9 PM MT. Hardcode.
- **v1.1 (device registry)**: Look up TOU schedule from device registry
  (TASK-037). Use the schedule's peak end time.
- **Fallback**: If no peak info available, use `now + 4 hours`.

v1.0 can hardcode since we only have one deployment on Xcel Colorado.

## Dependencies
**Blocked by**: TASK-048b (Charge Now latch — needs FLAG_CHARGE_NOW in uplinks)
**Blocks**: none

## Acceptance Criteria
- [ ] Decode Lambda detects `FLAG_CHARGE_NOW=1` and writes `charge_now_override_until` to sentinel
- [ ] Scheduler skips pause when `now < charge_now_override_until`
- [ ] Scheduler logs "Charge Now opt-out active" when suppressing
- [ ] Opt-out expires naturally after peak window ends
- [ ] Heartbeat re-send: scheduler re-sends if last_sent > 30 min ago
- [ ] Heartbeat suppressed during Charge Now opt-out
- [ ] Sentinel `charge_now_override_until` field cleared or ignored after expiry

## Testing Requirements
- [ ] Python tests: decode Lambda writes override_until on FLAG_CHARGE_NOW
- [ ] Python tests: scheduler skips pause during opt-out window
- [ ] Python tests: scheduler resumes after opt-out expires
- [ ] Python tests: heartbeat re-sends stale commands
- [ ] Python tests: heartbeat suppressed during opt-out

## Deliverables
- Modified `decode_evse_lambda.py` (Charge Now detection + sentinel write)
- Modified `charge_scheduler_lambda.py` (opt-out guard + heartbeat TTL)
- Python tests
- TDD section 8.1, 8.2 already updated (this task implements what's documented)
