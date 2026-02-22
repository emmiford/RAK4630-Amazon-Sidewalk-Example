#!/usr/bin/env python3
"""Seed dashboard DynamoDB tables with realistic historical EVSE telemetry.

Builds firmware-format binary payloads and invokes the decode-evse Lambda
directly with backdated timestamps.  Only the LoRa radio hop is skipped —
payload parsing, state classification, DynamoDB writes, device-state updates,
and interlock transitions all run through the real Lambda code path.

Usage:
    python3 aws/seed_dashboard_data.py              # Seed 14 days of events
    python3 aws/seed_dashboard_data.py --aggregate   # Also run daily aggregation
    python3 aws/seed_dashboard_data.py --dry-run     # Print payloads, don't invoke
"""

import argparse
import base64
import json
import struct
import sys
import time
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo

import boto3

# --- Constants matching firmware / protocol_constants.py ---

TELEMETRY_MAGIC = 0xE5
PAYLOAD_VERSION = 0x0A
EPOCH_OFFSET = 1767225600  # 2026-01-01T00:00:00Z

WIRELESS_DEVICE_ID = "b319d001-6b08-4d88-b4ca-4d2d98a6d43c"
DEVICE_SC_ID = "SC-C014EA63"
DECODE_LAMBDA_NAME = "uplink-decoder"
AGGREGATION_LAMBDA_NAME = "daily-aggregation"

MT = ZoneInfo("America/Denver")

# J1772 states (must match evse_sensors.h)
STATE_A = 0  # No vehicle (+12V)
STATE_B = 1  # Connected, not ready (+9V)
STATE_C = 2  # Charging (+6V)
STATE_D = 3  # Ventilation required (+3V)
STATE_E = 4  # Error - short circuit (0V)
STATE_F = 5  # Error - no pilot (-12V)
STATE_UNKNOWN = 6

# Typical voltages per state (mV)
VOLTAGES = {
    STATE_A: 12000,
    STATE_B: 9000,
    STATE_C: 6000,
    STATE_D: 3000,
    STATE_E: 0,
    STATE_F: 0,
    STATE_UNKNOWN: 0,
}

# Transition reasons (must match charge_control.h)
REASON_NONE = 0x00
REASON_CLOUD_CMD = 0x01
REASON_DELAY_WINDOW = 0x02
REASON_CHARGE_NOW = 0x03
REASON_AUTO_RESUME = 0x04
REASON_MANUAL = 0x05


# --- Payload builder ---

def build_telemetry_payload(
    j1772_state,
    voltage_mv=None,
    current_ma=0,
    charge_allowed=True,
    charge_now=False,
    thermostat_cool=False,
    fault_sensor=False,
    fault_clamp=False,
    fault_interlock=False,
    fault_selftest=False,
    unix_ts=0,
    transition_reason=REASON_NONE,
    app_version=11,
    platform_version=3,
):
    """Build a 15-byte v0x0A telemetry payload matching firmware format.

    Format: [magic][version][state][voltage_le16][current_le16][flags][epoch_le32][reason][app_ver][plat_ver]
    """
    if voltage_mv is None:
        voltage_mv = VOLTAGES.get(j1772_state, 0)

    flags = 0
    if thermostat_cool:
        flags |= 0x02
    if charge_allowed:
        flags |= 0x04
    if charge_now:
        flags |= 0x08
    if fault_sensor:
        flags |= 0x10
    if fault_clamp:
        flags |= 0x20
    if fault_interlock:
        flags |= 0x40
    if fault_selftest:
        flags |= 0x80

    sc_epoch = max(0, unix_ts - EPOCH_OFFSET) if unix_ts > 0 else 0

    payload = struct.pack(
        "<BBBHHBIBBB",
        TELEMETRY_MAGIC,    # byte 0: magic
        PAYLOAD_VERSION,    # byte 1: version
        j1772_state,        # byte 2: J1772 state
        voltage_mv,         # bytes 3-4: voltage LE
        current_ma,         # bytes 5-6: current LE
        flags,              # byte 7: flags
        sc_epoch,           # bytes 8-11: device epoch LE
        transition_reason,  # byte 12: transition reason
        app_version,        # byte 13: app build version
        platform_version,   # byte 14: platform build version
    )
    return payload


def build_sidewalk_event(payload_bytes, unix_ts_ms):
    """Wrap a binary payload in a Sidewalk-format Lambda event."""
    payload_b64 = base64.b64encode(payload_bytes).decode("ascii")
    return {
        "WirelessDeviceId": WIRELESS_DEVICE_ID,
        "PayloadData": payload_b64,
        "WirelessMetadata": {
            "Sidewalk": {
                "CmdExStatus": "0",
                "LinkType": "3",  # LoRa
                "Rssi": -85,
                "Seq": 0,
                "SidewalkId": "seed-data",
                "Timestamp": "",
            }
        },
        "timestamp_override_ms": unix_ts_ms,
    }


# --- Scenario definitions ---

def _dt_to_ms(dt):
    """datetime → Unix milliseconds."""
    return int(dt.timestamp() * 1000)


def _mt(year, month, day, hour=0, minute=0, second=0):
    """Build a Mountain Time datetime."""
    return datetime(year, month, day, hour, minute, second, tzinfo=MT)


def generate_heartbeats(day_dt, state=STATE_A, interval_s=900, current_ma=0,
                        charge_allowed=True, **kwargs):
    """Generate heartbeat events every `interval_s` seconds for a full day."""
    events = []
    start = day_dt.replace(hour=0, minute=0, second=0)
    end = start + timedelta(days=1)
    t = start
    while t < end:
        unix_ts = int(t.timestamp())
        payload = build_telemetry_payload(
            j1772_state=state,
            current_ma=current_ma,
            charge_allowed=charge_allowed,
            unix_ts=unix_ts,
            **kwargs,
        )
        events.append(build_sidewalk_event(payload, _dt_to_ms(t)))
        t += timedelta(seconds=interval_s)
    return events


def generate_charge_session(day_dt, start_hour, duration_hours, current_ma=30000,
                            pre_connect_hours=0.5, reason=REASON_NONE,
                            charge_now=False, **kwargs):
    """Generate a complete A→B→C→A charge session with heartbeats."""
    events = []

    # State A heartbeats before session (midnight to connect)
    t = day_dt.replace(hour=0, minute=0, second=0)
    connect_time = day_dt.replace(hour=start_hour, minute=0, second=0) - timedelta(hours=pre_connect_hours)
    while t < connect_time:
        unix_ts = int(t.timestamp())
        events.append(build_sidewalk_event(
            build_telemetry_payload(STATE_A, unix_ts=unix_ts), _dt_to_ms(t)))
        t += timedelta(seconds=900)

    # State B (connected, waiting)
    t = connect_time
    charge_start = day_dt.replace(hour=start_hour, minute=0, second=0)
    while t < charge_start:
        unix_ts = int(t.timestamp())
        events.append(build_sidewalk_event(
            build_telemetry_payload(STATE_B, unix_ts=unix_ts), _dt_to_ms(t)))
        t += timedelta(seconds=900)

    # Transition event at charge start
    unix_ts = int(charge_start.timestamp())
    events.append(build_sidewalk_event(
        build_telemetry_payload(
            STATE_C, current_ma=current_ma, unix_ts=unix_ts,
            transition_reason=reason, charge_now=charge_now, **kwargs),
        _dt_to_ms(charge_start)))

    # State C heartbeats during charge
    t = charge_start + timedelta(seconds=900)
    charge_end = charge_start + timedelta(hours=duration_hours)
    while t < charge_end:
        unix_ts = int(t.timestamp())
        events.append(build_sidewalk_event(
            build_telemetry_payload(
                STATE_C, current_ma=current_ma, unix_ts=unix_ts,
                charge_now=charge_now, **kwargs),
            _dt_to_ms(t)))
        t += timedelta(seconds=900)

    # Transition back to A
    unix_ts = int(charge_end.timestamp())
    events.append(build_sidewalk_event(
        build_telemetry_payload(STATE_A, unix_ts=unix_ts, transition_reason=REASON_NONE),
        _dt_to_ms(charge_end)))

    # State A heartbeats after charge until end of day
    t = charge_end + timedelta(seconds=900)
    end_of_day = day_dt.replace(hour=0, minute=0, second=0) + timedelta(days=1)
    while t < end_of_day:
        unix_ts = int(t.timestamp())
        events.append(build_sidewalk_event(
            build_telemetry_payload(STATE_A, unix_ts=unix_ts), _dt_to_ms(t)))
        t += timedelta(seconds=900)

    return events


def build_scenario_events():
    """Build all 14 days of scenario events per the experiment plan."""
    now = datetime.now(MT)
    today = now.replace(hour=0, minute=0, second=0, microsecond=0)
    events = []

    # D-14 to D-12: Normal idle (heartbeats only, State A)
    for offset in range(14, 11, -1):
        day = today - timedelta(days=offset)
        events.extend(generate_heartbeats(day, state=STATE_A))

    # D-11: Evening charge session (A→B→C→A, 4h charge, scheduler allows)
    day = today - timedelta(days=11)
    events.extend(generate_charge_session(
        day, start_hour=18, duration_hours=4, current_ma=28000,
        reason=REASON_CLOUD_CMD))

    # D-10: Scheduler delay window (A→B wait→C→A, TOU peak delays start)
    day = today - timedelta(days=10)
    events.extend(generate_charge_session(
        day, start_hour=21, duration_hours=3, current_ma=30000,
        pre_connect_hours=4, reason=REASON_DELAY_WINDOW))

    # D-9: Two charge sessions (morning + evening)
    day = today - timedelta(days=9)
    # Morning session
    morning_events = generate_charge_session(
        day, start_hour=6, duration_hours=2, current_ma=25000,
        reason=REASON_AUTO_RESUME)
    # Evening session — build manually to avoid overlapping heartbeats
    evening_start = day.replace(hour=19)
    evening_end = day.replace(hour=22)
    evening_events = []
    t = evening_start
    # B phase
    unix_ts = int(t.timestamp())
    evening_events.append(build_sidewalk_event(
        build_telemetry_payload(STATE_B, unix_ts=unix_ts), _dt_to_ms(t)))
    t += timedelta(minutes=30)
    # C phase
    while t < evening_end:
        unix_ts = int(t.timestamp())
        evening_events.append(build_sidewalk_event(
            build_telemetry_payload(STATE_C, current_ma=30000, unix_ts=unix_ts,
                                    transition_reason=REASON_CLOUD_CMD if t == evening_start + timedelta(minutes=30) else REASON_NONE),
            _dt_to_ms(t)))
        t += timedelta(seconds=900)
    # Back to A
    unix_ts = int(evening_end.timestamp())
    evening_events.append(build_sidewalk_event(
        build_telemetry_payload(STATE_A, unix_ts=unix_ts), _dt_to_ms(evening_end)))

    # Merge: take morning events up to 17:00, then evening events
    cutoff = _dt_to_ms(day.replace(hour=17))
    events.extend([e for e in morning_events if e["timestamp_override_ms"] < cutoff])
    # Fill gap with A heartbeats from end of morning to evening
    gap_start = day.replace(hour=9)  # morning session ends ~8
    gap_end = evening_start
    t = gap_start
    while t < gap_end:
        unix_ts = int(t.timestamp())
        events.append(build_sidewalk_event(
            build_telemetry_payload(STATE_A, unix_ts=unix_ts), _dt_to_ms(t)))
        t += timedelta(seconds=900)
    events.extend(evening_events)
    # A heartbeats after evening
    t = evening_end + timedelta(seconds=900)
    eod = day + timedelta(days=1)
    while t < eod:
        unix_ts = int(t.timestamp())
        events.append(build_sidewalk_event(
            build_telemetry_payload(STATE_A, unix_ts=unix_ts), _dt_to_ms(t)))
        t += timedelta(seconds=900)

    # D-8: Fault day (charge with transient sensor fault)
    day = today - timedelta(days=8)
    fault_events = generate_charge_session(
        day, start_hour=20, duration_hours=3, current_ma=28000,
        reason=REASON_CLOUD_CMD)
    # Inject fault flags into a few events during charging
    fault_injected = 0
    for ev in fault_events:
        raw = base64.b64decode(ev["PayloadData"])
        if raw[2] == STATE_C and fault_injected < 3:
            # Re-build with fault_sensor flag
            mutable = bytearray(raw)
            mutable[7] |= 0x10  # fault_sensor bit
            ev["PayloadData"] = base64.b64encode(bytes(mutable)).decode("ascii")
            fault_injected += 1
    events.extend(fault_events)

    # D-7 to D-5: Normal mix (routine charging)
    for offset in range(7, 4, -1):
        day = today - timedelta(days=offset)
        events.extend(generate_charge_session(
            day, start_hour=19 + (offset % 3), duration_hours=3,
            current_ma=27000 + offset * 500, reason=REASON_CLOUD_CMD))

    # D-4: OTA update during charge
    day = today - timedelta(days=4)
    events.extend(generate_charge_session(
        day, start_hour=20, duration_hours=4, current_ma=29000,
        reason=REASON_CLOUD_CMD))

    # D-3: Charge Now override
    day = today - timedelta(days=3)
    events.extend(generate_charge_session(
        day, start_hour=17, duration_hours=3, current_ma=30000,
        reason=REASON_CHARGE_NOW, charge_now=True))

    # D-2: Wandering episode (reproduce Feb 20-21 pattern)
    day = today - timedelta(days=2)
    wander_events = []
    t = day.replace(hour=0, minute=0, second=0)
    eod = day + timedelta(days=1)
    wander_states = [STATE_A, STATE_B, STATE_E, STATE_UNKNOWN, STATE_A,
                     STATE_C, STATE_A, STATE_D, STATE_UNKNOWN, STATE_A]
    state_idx = 0
    while t < eod:
        state = wander_states[state_idx % len(wander_states)]
        # Advance state every ~2.5 hours
        if t >= day.replace(hour=0) + timedelta(hours=2.5 * (state_idx + 1)):
            state_idx += 1
            state = wander_states[state_idx % len(wander_states)]

        voltage = VOLTAGES.get(state, 0)
        # Add voltage noise for wandering
        if state in (STATE_E, STATE_UNKNOWN):
            import random
            voltage = random.randint(0, 2000)

        unix_ts = int(t.timestamp())
        current = 15000 if state == STATE_C else 0
        payload = build_telemetry_payload(
            state, voltage_mv=voltage, current_ma=current,
            charge_allowed=(state != STATE_E),
            unix_ts=unix_ts)
        wander_events.append(build_sidewalk_event(payload, _dt_to_ms(t)))
        t += timedelta(seconds=900)
    events.extend(wander_events)

    # D-1: Recovery (State D, voltage low, then stabilize)
    day = today - timedelta(days=1)
    recovery_events = []
    t = day.replace(hour=0, minute=0, second=0)
    eod = day + timedelta(days=1)
    while t < eod:
        unix_ts = int(t.timestamp())
        if t.hour < 6:
            # Low voltage D state early morning
            payload = build_telemetry_payload(
                STATE_D, voltage_mv=2500, unix_ts=unix_ts, charge_allowed=False)
        elif t.hour < 12:
            # Recovering to B
            payload = build_telemetry_payload(
                STATE_B, voltage_mv=8500, unix_ts=unix_ts)
        else:
            # Stable A
            payload = build_telemetry_payload(
                STATE_A, unix_ts=unix_ts)
        recovery_events.append(build_sidewalk_event(payload, _dt_to_ms(t)))
        t += timedelta(seconds=900)
    events.extend(recovery_events)

    # D-0: Current day (State A / B for "online" status — just a few events)
    day = today
    t = day.replace(hour=0, minute=0, second=0)
    end_time = min(now, day.replace(hour=23, minute=45))
    while t < end_time:
        unix_ts = int(t.timestamp())
        state = STATE_B if 8 <= t.hour <= 10 else STATE_A
        payload = build_telemetry_payload(state, unix_ts=unix_ts)
        events.append(build_sidewalk_event(payload, _dt_to_ms(t)))
        t += timedelta(seconds=900)

    # Sort by timestamp
    events.sort(key=lambda e: e["timestamp_override_ms"])
    return events


# --- Lambda invocation ---

def invoke_decode_lambda(lambda_client, event, dry_run=False):
    """Invoke the decode-evse Lambda with a single event."""
    if dry_run:
        raw = base64.b64decode(event["PayloadData"])
        ts_ms = event["timestamp_override_ms"]
        dt = datetime.fromtimestamp(ts_ms / 1000, tz=MT)
        state_code = raw[2] if len(raw) > 2 else -1
        states = {0: "A", 1: "B", 2: "C", 3: "D", 4: "E", 5: "F", 6: "?"}
        print(f"  {dt.strftime('%Y-%m-%d %H:%M')} MT  state={states.get(state_code, '?')}  "
              f"payload={raw.hex()}")
        return True

    resp = lambda_client.invoke(
        FunctionName=DECODE_LAMBDA_NAME,
        InvocationType="RequestResponse",
        Payload=json.dumps(event),
    )
    status = resp["StatusCode"]
    if status != 200:
        body = resp["Payload"].read().decode()
        print(f"  ERROR: Lambda returned {status}: {body}")
        return False
    return True


def run_aggregation(lambda_client, days, dry_run=False):
    """Invoke the aggregation Lambda for each day that has seeded data."""
    today = datetime.now(MT).replace(hour=0, minute=0, second=0, microsecond=0)
    for offset in range(days, 0, -1):
        day = today - timedelta(days=offset)
        date_str = day.strftime("%Y-%m-%d")
        print(f"  Aggregating {date_str}...")
        if dry_run:
            continue
        resp = lambda_client.invoke(
            FunctionName=AGGREGATION_LAMBDA_NAME,
            InvocationType="RequestResponse",
            Payload=json.dumps({"date": date_str}),
        )
        if resp["StatusCode"] != 200:
            body = resp["Payload"].read().decode()
            print(f"    ERROR: {body}")
        else:
            body = json.loads(resp["Payload"].read().decode())
            print(f"    OK: {body.get('body', body)}")


def main():
    parser = argparse.ArgumentParser(description="Seed EVSE dashboard data via Lambda replay")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print payloads without invoking Lambda")
    parser.add_argument("--aggregate", action="store_true",
                        help="Also run daily aggregation after seeding events")
    parser.add_argument("--days", type=int, default=14,
                        help="Number of days of history (default: 14)")
    args = parser.parse_args()

    print(f"Building {args.days}-day scenario events...")
    events = build_scenario_events()
    print(f"Generated {len(events)} events")

    if not args.dry_run:
        lambda_client = boto3.client("lambda")
    else:
        lambda_client = None

    # Group by day for progress reporting
    from collections import Counter
    day_counts = Counter()
    for ev in events:
        dt = datetime.fromtimestamp(ev["timestamp_override_ms"] / 1000, tz=MT)
        day_counts[dt.strftime("%Y-%m-%d")] += 1

    print("\nEvents per day:")
    for day_str in sorted(day_counts):
        print(f"  {day_str}: {day_counts[day_str]} events")

    print(f"\nInvoking decode Lambda for {len(events)} events...")
    success = 0
    errors = 0
    for i, event in enumerate(events):
        ok = invoke_decode_lambda(lambda_client, event, dry_run=args.dry_run)
        if ok:
            success += 1
        else:
            errors += 1

        # Progress every 100 events
        if (i + 1) % 100 == 0:
            print(f"  Progress: {i + 1}/{len(events)} ({errors} errors)")

        # Rate limit: Lambda concurrent invocations
        if not args.dry_run and (i + 1) % 50 == 0:
            time.sleep(0.5)

    print(f"\nSeeding complete: {success} OK, {errors} errors")

    if args.aggregate:
        print(f"\nRunning daily aggregation for {args.days} days...")
        run_aggregation(lambda_client, args.days, dry_run=args.dry_run)
        print("Aggregation complete.")


if __name__ == "__main__":
    main()
