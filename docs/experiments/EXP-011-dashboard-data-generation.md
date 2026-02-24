# EXP-011: Dashboard Data Generation — Lambda Replay + Serial Automation

**Status**: Concluded
**Verdict**: GO
**Type**: Tooling
**Date**: 2026-02-22
**Owner**: Oliver
**Related**: TASK-108

---

## Problem Statement

The EVSE Fleet Dashboard needed realistic multi-day telemetry data for development and testing, but accumulating real device data over LoRa would take weeks. Without representative data (charge sessions, faults, idle periods, OTA events, Charge Now overrides), dashboard features couldn't be validated.

## Hypothesis

Invoking the decode Lambda directly with firmware-format binary payloads and backdated timestamps will generate realistic dashboard data that exercises the full decode pipeline — bypassing only the LoRa radio hop.

**Success Metrics**:
- Primary: DynamoDB populated with 14 days of realistic telemetry
- Secondary: dashboard renders all event types correctly; full pipeline verified via serial automation

## Method

**Branch**: Part of `task/106-dashboard-migration` work
**Implementation** (commit `cd08a27`):

**Variant A — Lambda replay** (`aws/seed_dashboard_data.py`):
- Builds firmware-format binary payloads (magic byte, version, J1772 state, pilot voltage, current, thermostat flags, device epoch)
- Invokes the `uplink-decoder` Lambda directly with `timestamp_override_ms` for backdating
- Generates 14 days of scenarios: idle, charge sessions, faults, wandering pilot, recovery, OTA, Charge Now overrides
- Small Lambda change: `decode_evse_lambda.py` accepts optional `timestamp_override_ms` field

**Variant B — Serial automation** (`aws/serial_data_gen.py`):
- Connects to device serial port (`/dev/tty.usbmodem101`)
- Cycles through J1772 simulation states via shell commands (`app evse a/b/c`, `app evse allow/pause`, `app sid send`)
- Each uplink travels the full path: device → LoRa → Sidewalk → IoT Rule → decode Lambda → DynamoDB
- Respects the 5s TX rate limiter

## Results

**Decision**: GO — both approaches work, committed to main
**Primary Metric Impact**: 14 days of realistic telemetry populated in ~2 minutes (Lambda replay) vs real-time (serial automation).
**Bug discovered**: Seed script exposed TASK-108 — `ev.timestamp` vs `ev.timestamp_mt` field mismatch in dashboard frontend.
**Secondary benefit**: `timestamp_override_ms` approach keeps Lambda as single source of truth for payload decoding.

## Key Insights

- Lambda replay that uses the real decode path is strictly better than writing DynamoDB records directly — catches schema mismatches, decode bugs, and interlock logic issues.
- The seed script's scenario library doubles as a regression suite for the decode pipeline.
- Serial automation is slower but validates the full radio path. Use Lambda replay for volume, serial for end-to-end confidence.
- The `timestamp_override_ms` pattern is a clean way to enable historical data injection without polluting the production code path.

## References

- Files: `aws/seed_dashboard_data.py`, `aws/serial_data_gen.py`
