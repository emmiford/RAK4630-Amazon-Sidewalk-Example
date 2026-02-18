# TASK-075: Delay Window On-Device Verification

**Date**: 2026-02-17
**Firmware**: task/072-button-gpio-verify app on main platform (app.hex flashed via pyOCD)
**Tester**: Eero (cloud automation + manual serial)
**Device**: RAK4631 on RAK5005-O baseboard, Sidewalk LoRa link

## Summary

Core delay window functionality verified end-to-end over LoRa. Device correctly
pauses charging on window receipt, resumes autonomously on expiry, and legacy
ALLOW cancels an active window mid-flight. One edge-case test (pre-TIME_SYNC
ignore) was not achievable due to LoRa timing but is covered by unit tests.

## Test Results

| # | Test | Result | Notes |
|---|------|--------|-------|
| 1 | TIME_SYNC present (non-zero epoch) | PASS | `ts=4161490` in uplink (~50s behind real time) |
| 2 | Delay window pauses charging | PASS | Sent 120s window → `Charging allowed: NO` within 30s |
| 3 | Device resumes on window expiry | PASS | After 120s + clock offset → `Charging allowed: YES` |
| 4 | 5-min delay window pauses charging | PASS | Sent 300s window → `Charging allowed: NO` |
| 5 | Legacy ALLOW cancels active 30-min window | PASS | Sent 1800s window, confirmed paused, then ALLOW cancelled with ~26 min remaining. Log: `Delay window cleared` + `Charge control: ALLOW` → `Charging allowed: YES` |
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

### Test 5: Legacy ALLOW cancels active 30-min delay window

```
# 30-min delay window sent: start=4162152, end=4163952 (1800s)
[00:20:58.463] sidewalk_dispatch: Received message:
               10 02 68 82 3f 00 70 89 3f 00
[00:20:58.467] platform_api: Delay window command received
[00:20:58.468] platform_api: Delay window: start=4162152 end=4163952 (duration=1800s)

# Confirmed paused:
app evse status → Charging allowed: NO

# ALLOW sent to cancel (window has ~26 min remaining):
[00:24:43.451] sidewalk_dispatch: Received message: 10 01 00 00
[00:24:43.455] platform_api: Charge control command received
[00:24:43.457] platform_api: Delay window cleared        ← cancel confirmed!
[00:24:43.458] platform_api: Charge control command: allowed=1, duration=0 min
[00:24:43.461] platform_api: Charge control: ALLOW

# Confirmed resumed:
app evse status → Charging allowed: YES
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
- [x] Legacy allow command cancels active delay window (30-min window cancelled
      with ~26 min remaining; `Delay window cleared` logged)
- [~] Device ignores delay window when time is not synced (not testable in practice —
      TIME_SYNC arrives before delay window; code path + unit test confirmed)
- [ ] Scheduler integration: EventBridge-triggered scheduler sends delay window during
      TOU peak (not tested — requires live peak window)
