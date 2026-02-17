# TASK-063: Delay window support — device storage + scheduler format change

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: L (8 points)

## Description
Replace the current pause/allow command model with delay windows per PRD 4.4.1.
This is a combined device + cloud task.

### Problem
The current charge scheduler sends fire-and-forget "pause" and "allow" commands.
If the "allow" is lost over LoRa, the device stays paused indefinitely. The
sentinel (timestamp=0) tracks the last command sent but has no device ACK, so
lost downlinks cause silent divergence (see ADR-003 caveat).

### Solution
The cloud sends **delay windows** `[start_time, end_time]` in SideCharge epoch.
The device stores the window and manages transitions autonomously. When
`now > end_time`, charging resumes — no cloud message needed.

### Device side
1. Parse delay window from charge control downlink (0x10) — new format with
   start/end timestamps instead of pause/allow byte
2. Store one active window in RAM (new downlink replaces previous)
3. On each poll cycle: if `start ≤ now ≤ end`, pause; else allow
4. When Charge Now is pressed (TASK-048b), delete the stored window
5. Requires TIME_SYNC to be operational (device needs wall-clock time)

### Cloud side
1. Scheduler sends `[start_time, end_time]` in charge control downlink
2. For TOU: daily window (e.g., 5 PM–9 PM in SideCharge epoch)
3. For MOER: shorter dynamic windows (15–60 min)
4. Sentinel (timestamp=0) updated to track window sent, not command
5. Heartbeat re-send: if sentinel window is >N min old and peak is still
   active, re-send the window (safe because device handles expiry)

### Wire format (proposed)
Extend charge control downlink (0x10) — backward-compatible:
```
Byte 0:   0x10 (CHARGE_CONTROL_CMD_TYPE)
Byte 1:   0x02 (subtype: delay_window — 0x00=legacy pause/allow, 0x02=window)
Byte 2-5: start_time (uint32_le, SideCharge epoch seconds)
Byte 6-9: end_time (uint32_le, SideCharge epoch seconds)
```
Total: 10 bytes (within 19-byte LoRa MTU)

Old firmware ignores unknown subtypes (existing `cmd_type` check passes but
`charge_allowed` byte would be 0x02, treating it as "allow" — safe fallback).

## Dependencies
**Blocked by**: TASK-033 (TIME_SYNC — DONE), TASK-035 (uplink v0x07 — DONE)
**Blocks**: none (TASK-048b can land first with "ignore cloud pause" behavior;
delay window deletion added when this task merges)

## Acceptance Criteria
- [ ] Device parses delay window downlink and stores `[start, end]` in RAM
- [ ] Device pauses charging when `start ≤ now ≤ end`, resumes when `now > end`
- [ ] New downlink replaces previous window (one window at a time)
- [ ] Device with no TIME_SYNC (epoch=0) ignores delay windows (safe default)
- [ ] Scheduler sends delay windows instead of pause/allow for TOU and MOER
- [ ] Scheduler sentinel updated to track window boundaries
- [ ] Heartbeat re-send: scheduler re-sends window if stale (>30 min since last send)
- [ ] Legacy pause/allow commands still work (subtype 0x00 or absent)
- [ ] Python Lambda tests updated for new downlink format
- [ ] C unit tests for window parsing, storage, expiry, and replacement

## Testing Requirements
- [ ] C unit tests: window active → charging paused
- [ ] C unit tests: window expired → charging allowed
- [ ] C unit tests: new window replaces old
- [ ] C unit tests: no TIME_SYNC → window ignored
- [ ] C unit tests: legacy command still works
- [ ] Python tests: scheduler sends window format
- [ ] Python tests: heartbeat re-send logic
- [ ] Python tests: sentinel tracks window boundaries

## Deliverables
- New `delay_window.c` / `delay_window.h` (device-side window storage + check)
- Modified `charge_control.c` (integrate window check into poll cycle)
- Modified `app_rx.c` (parse new downlink format)
- Modified `charge_scheduler_lambda.py` (send windows, heartbeat, sentinel update)
- Modified `decode_evse_lambda.py` (decode new downlink in tests)
- Unit tests (C + Python)
- TDD section 4.1 updated with new wire format
