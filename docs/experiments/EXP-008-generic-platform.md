# EXP-008: Generic Platform (Move All Domain Knowledge to App)

**Status**: Concluded
**Verdict**: GO
**Type**: Architecture change
**Date**: pre-2026-02
**Owner**: Oliver
**Related**: EXP-007 (predecessor), ADR-001

---

## Problem Statement

The platform still contained EVSE-specific knowledge (sensor_monitor.c, PIN definitions, evse/hvac shell commands). This meant platform updates were needed for domain logic changes, defeating the purpose of app-only OTA.

## Hypothesis

Moving ALL domain-specific code (sensor interpretation, pin assignments, change detection, polling, shell commands) to the app partition will make the platform truly generic and the app fully self-contained and OTA-updatable.

**Success Metrics**:
- Primary: platform has zero EVSE knowledge; app is independently buildable and testable; all existing functionality preserved

## Method

**Implementation** (commit `e88d519`):
- Delete `sensor_monitor.c` from platform
- Add `set_timer_interval()` to platform API (app configures its own 500ms poll)
- Move PIN defines from platform to app modules
- Replace evse/hvac shell commands with generic "app" shell dispatch
- App does own change detection + 60s heartbeat in `on_timer()`
- Add host-side unit test harness (Grenning dual-target pattern, 32 tests)

## Results

**Decision**: GO — completed on `feature/generic-platform`, merged to main
**Primary Metric Impact**: Platform is now fully domain-agnostic. App contains all EVSE logic.
**Test Coverage**: 32 host-side unit tests covering sensors, thermostat, charge control, TX, and on_timer change detection.
**API Contract**: Platform API v2 (magic 0x504C4154), App Callbacks v3 (magic 0x53415050)

## Key Insights

- The Grenning dual-target testing pattern was the key enabler — without host-side tests, this refactor would have been much riskier.
- `set_timer_interval()` was the crucial missing API — app needs to control its own polling rate without platform knowledge.
- Generic shell dispatch (`on_shell_cmd` callback) lets the app register arbitrary commands without platform changes.
- This completes the split-image vision from EXP-007: platform is now a reusable Sidewalk sensor runtime.
