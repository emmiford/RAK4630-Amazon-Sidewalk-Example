# TASK-048b: Charge Now 30-minute latch — device-side override logic

**Status**: committed (2026-02-17, Eliel)
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/048b-charge-now-latch`
**Size**: M (5 points)

## Description
Implement the Charge Now 30-minute latch per ADR-003 and PRD 2.0.1.1. When the
Charge Now button is single-pressed, the device enters a 30-minute override mode:

1. **Charging forced on** — `charge_allowed = true`, relay held closed
2. **AC suppressed** — thermostat cool call is blocked for 30 minutes
3. **Cloud pause commands ignored** — incoming charge control (0x10) pause
   commands are discarded while the latch is active
4. **FLAG_CHARGE_NOW set** — bit 3 of uplink flags byte is held high for the
   duration (not auto-cleared after one uplink)
5. **Delay window deleted** — once delay window support exists (TASK-063), the
   stored window is deleted on Charge Now press

On expiry (or early cancel via unplug / car full / long-press):
- AC priority restored, normal interlock rules resume
- FLAG_CHARGE_NOW cleared in next uplink
- No demand response window reinstated — cloud handles opt-out (TASK-064)

This replaces the earlier "toggle + auto-clear" scope that was in the original
TASK-048b. See ADR-003 for the decision rationale.

## Dependencies
**Blocked by**: TASK-062 (button GPIO wiring)
**Blocks**: TASK-064 (cloud Charge Now protocol — needs FLAG_CHARGE_NOW in uplinks)

## Acceptance Criteria
- [x] Single press activates 30-min Charge Now latch
- [x] During latch: charging forced on, cool call suppressed, cloud pause ignored
- [x] FLAG_CHARGE_NOW (bit 3) set in uplinks for full 30-min duration
- [x] FLAG_CHARGE_NOW clears when latch expires or is cancelled
- [x] Unplug (J1772 → state A) cancels latch immediately
- [x] Long-press (3s) cancels latch early
- [x] Coexists with 5-press self-test trigger (no false triggers)
- [x] Power loss = latch lost (RAM-only), safe default restored
- [x] LED feedback: 3 rapid blinks on press, then 0.5Hz slow blink during override

## Testing Requirements
- [x] C unit tests: single press activates latch, sets FLAG_CHARGE_NOW
- [x] C unit tests: cloud pause command ignored during latch
- [x] C unit tests: cool call suppressed during latch, restored after expiry
- [x] C unit tests: unplug cancels latch
- [x] C unit tests: 30-min expiry restores normal operation
- [x] C unit tests: 5-press self-test still works (no regression)
- [ ] On-device: button press → LED feedback → charging starts

## Deliverables
- New `charge_now.c` / `charge_now.h` (latch state machine, timer, cancel logic)
- Modified `charge_control.c` (check latch before processing cloud commands)
- Modified `app_tx.c` (FLAG_CHARGE_NOW from latch state, not auto-clear)
- Modified thermostat handling (suppress cool call during latch)
- Unit tests
