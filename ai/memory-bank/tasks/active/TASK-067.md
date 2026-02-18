# TASK-067: LED blink priority state machine

**Status**: committed (2026-02-17, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: task/067-led-blink-engine
**Size**: M (3 points)

## Description
Implement the LED blink priority state machine specified in PRD §2.5.1. The
device currently has no runtime LED patterns — LEDs are only used during
button-triggered self-test blink codes. The PRD specifies 8 priority levels
with distinct blink patterns for the single-LED prototype, and a dual-LED
matrix for production (§2.5.1.1).

This is the primary visual interface for both installers and homeowners. Without
it, boot self-test failures are invisible (just a single instantaneous flash),
error states have no visual indicator, and there's no "device is alive" heartbeat.

**Scope (prototype — single green LED)**:
1. Priority-based blink engine driven by 100ms timer tick (decimated to 500ms for sensors)
2. Patterns per §2.5.1 matrix (error=5Hz, OTA=double-blink, commissioning=1Hz, etc.)
3. Commissioning mode auto-exit on first successful Sidewalk uplink or 5-min timeout
4. Error state entry criteria per PRD §2.5.1 (ADC fail 3x, GPIO fail 3x, sidewalk 10-min timeout)
5. Charge Now button acknowledgment (3 rapid blinks)
6. Self-test blink codes continue to work (selftest_trigger.c takes over LEDs temporarily)

**Out of scope**: Production dual-LED patterns (requires TASK-019 PCB).

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] Blink engine runs in app layer, driven by 100ms `on_timer` tick (500ms decimation for sensors)
- [x] All 8 priority levels from PRD §2.5.1 implemented
- [x] Highest-priority active state wins when multiple states are active
- [x] Commissioning mode (1Hz) starts on boot, exits on first successful uplink or 5-min timeout
- [x] Error/fault state triggers 5Hz rapid flash per entry criteria
- [x] Self-test blink codes override normal patterns during test, restore after
- [x] Idle heartbeat blip (100ms every 10s) confirms device is alive
- [x] Unit tests for priority resolution and state transitions

## Testing Requirements
- [x] Unit tests for blink state machine priority resolution (10 tests)
- [x] Unit tests for commissioning mode entry/exit (2 tests)
- [x] Unit tests for error state entry criteria (4 tests: 3x ADC fail, success resets, sidewalk timeout, timeout clears)
- [ ] On-device verification: observe correct LED patterns for each state

## Deliverables
- New source file: `src/app_evse/led_engine.c`
- New header: `include/led_engine.h`
- Integration in `app_entry.c::app_on_timer()` — timer 500ms → 100ms with 5-count decimation
- 21 new unit tests (96 total, all passing)
- Mock LED call history array for pattern verification
- CMakeLists.txt + Makefile updated
