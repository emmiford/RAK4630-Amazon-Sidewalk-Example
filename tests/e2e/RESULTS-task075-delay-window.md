# TASK-075: Delay Window On-Device Verification

**Date**: 2026-02-17
**Firmware**: task/072-button-gpio-verify app on main platform (app.hex flashed via pyOCD)
**Tester**: Eero (cloud automation + manual serial)
**Device**: RAK4631 on RAK5005-O baseboard, Sidewalk LoRa link

## Summary

Core delay window functionality verified end-to-end over LoRa. Device correctly
pauses charging on window receipt, resumes autonomously on expiry, and processes
legacy ALLOW commands. Two edge-case tests were not achievable due to LoRa timing
constraints but are covered by unit tests and code review.

## Test Results

| # | Test | Result | Notes |
|---|------|--------|-------|
| 1 | TIME_SYNC present (non-zero epoch) | PASS | `ts=4161490` in uplink (~50s behind real time) |
| 2 | Delay window pauses charging | PASS | Sent 120s window → `Charging allowed: NO` within 30s |
| 3 | Device resumes on window expiry | PASS | After 120s + clock offset → `Charging allowed: YES` |
| 4 | 5-min delay window pauses charging | PASS | Sent 300s window → `Charging allowed: NO` |
| 5 | Legacy ALLOW cancels active window | INCONCLUSIVE | Window expired naturally 40s before ALLOW arrived (PSA -149 delayed delivery). ALLOW was received and processed correctly. Code path confirmed: `charge_control_process_cmd()` calls `delay_window_clear()` at line 55. Unit test `test_cc_legacy_cmd_clears_window` passes. |
| 6 | Device ignores delay window before TIME_SYNC | NOT TESTED | TIME_SYNC arrived before delay window on fresh boot (~10 min uptime). Code path confirmed: `delay_window_is_paused()` returns false when `time_sync_get_epoch() == 0`. Unit test `test_delay_window_ignored_without_time_sync` passes. |
| 7 | Scheduler integration (EventBridge) | NOT TESTED | Requires TOU peak or high MOER to trigger naturally |

## Detailed Log Excerpts

### Test 2+3: Delay window pause and auto-resume

```
# ALLOW sent first to establish baseline
Charging allowed: YES

# Delay window sent: start=4159003, end=4159123 (120s)
TX: 10021b763f0093763f00 (10B)

# ~30s later:
Charging allowed: NO   ← paused by delay window

# After window expiry (+ ~50s clock offset):
EVSE TX v08: ... ts=4159196   ← device clock past window end (4159123)
Charging allowed: YES          ← auto-resumed
```

### Test 4+5: Long window + ALLOW cancel attempt

```
# 5-min delay window sent: start=4161507, end=4161807
[00:10:13.480] sidewalk_dispatch: Received message:
               10 02 e3 7f 3f 00 0f 81 3f 00
[00:10:13.484] platform_api: Delay window command received
[00:10:13.486] platform_api: Delay window: start=4161507 end=4161807 (duration=300s)

# Window expired naturally before ALLOW arrived:
[00:15:18.482] platform_api: Delay window expired, resuming
[00:15:18.483] platform_api: Delay window cleared

# ALLOW arrived via BLE ~40s after expiry:
[00:15:59.853] sidewalk_dispatch: Received message: 10 01 00 00
[00:15:59.856] platform_api: Charge control command received
[00:15:59.858] platform_api: Charge control command: allowed=1, duration=0 min
[00:15:59.860] platform_api: Charge control: ALLOW
```

## Known Issues Encountered

- **KI-002 (PSA crypto -149)**: Intermittently drops downlink payloads. Some messages
  arrive with hex dump and are processed; others show `Received message:` with no
  payload. This affected ALLOW delivery timing for test 5. See `docs/known-issues.md`.
- **BLE opportunistic probes**: `BT Connected` / `BT Disconnected` messages during
  LoRa session are normal Sidewalk stack behavior. The ALLOW was ultimately delivered
  over a BLE connection, not LoRa.
- **SID_ERROR_ALREADY_EXISTS (-27)**: Benign race — connection already open when
  reconnect was attempted.

## Acceptance Criteria Status

- [x] Device pauses charging when active delay window is received
- [x] Device resumes autonomously when delay window expires
- [~] Legacy allow command cancels active delay window (delivered correctly but after
      natural expiry; code path + unit test confirmed)
- [~] Device ignores delay window when time is not synced (not testable in practice —
      TIME_SYNC arrives before delay window; code path + unit test confirmed)
- [ ] Scheduler integration: EventBridge-triggered scheduler sends delay window during
      TOU peak (not tested — requires live peak window)
