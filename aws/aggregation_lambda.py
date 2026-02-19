"""Daily aggregation Lambda — computes per-device daily summaries from raw telemetry.

Runs on an EventBridge daily schedule. For each active device, queries the previous
day's telemetry events, computes energy, fault, and availability metrics, and writes
a summary record to the sidecharge-daily-aggregates table with a 3-year TTL.

Supports a 'date' override in the event payload for backfilling:
    {"date": "2026-02-18"}
Defaults to yesterday (UTC).

Environment variables:
    DYNAMODB_TABLE: Source telemetry events table (default: sidewalk-v1-device_events_v2)
    DEVICE_REGISTRY_TABLE: Device registry table (default: sidecharge-device-registry)
    AGGREGATES_TABLE: Destination aggregates table (default: sidecharge-daily-aggregates)
    ASSUMED_VOLTAGE_V: Assumed mains voltage for kWh calculation (default: 240)
"""

import json
import os
from datetime import datetime, timedelta, timezone

import boto3

dynamodb = boto3.resource("dynamodb")

events_table_name = os.environ.get("DYNAMODB_TABLE", "sidewalk-v1-device_events_v2")
registry_table_name = os.environ.get("DEVICE_REGISTRY_TABLE", "sidecharge-device-registry")
aggregates_table_name = os.environ.get("AGGREGATES_TABLE", "sidecharge-daily-aggregates")
assumed_voltage_v = int(os.environ.get("ASSUMED_VOLTAGE_V", "240"))

events_table = dynamodb.Table(events_table_name)
registry_table = dynamodb.Table(registry_table_name)
aggregates_table = dynamodb.Table(aggregates_table_name)

# Expected uplinks per day at 15-min heartbeat interval
EXPECTED_UPLINKS_PER_DAY = 96

# 3 years in seconds
THREE_YEAR_TTL_S = 94_608_000


# --- Device registry ---

def get_all_active_devices():
    """Scan device registry for all active devices.

    Returns list of device records with device_id and wireless_device_id.
    """
    devices = []
    scan_kwargs = {
        "FilterExpression": "#s = :active",
        "ExpressionAttributeNames": {"#s": "status"},
        "ExpressionAttributeValues": {":active": "active"},
    }

    while True:
        resp = registry_table.scan(**scan_kwargs)
        devices.extend(resp.get("Items", []))
        last_key = resp.get("LastEvaluatedKey")
        if not last_key:
            break
        scan_kwargs["ExclusiveStartKey"] = last_key

    return devices


# --- Telemetry query ---

def query_device_events(wireless_device_id, start_ms, end_ms):
    """Query evse_telemetry events for a device in a time range.

    Returns list of DynamoDB items sorted by timestamp ascending.
    """
    events = []
    query_kwargs = {
        "KeyConditionExpression": "#did = :did AND #ts BETWEEN :start AND :end",
        "FilterExpression": "#et = :telemetry",
        "ExpressionAttributeNames": {
            "#did": "device_id",
            "#ts": "timestamp",
            "#et": "event_type",
        },
        "ExpressionAttributeValues": {
            ":did": wireless_device_id,
            ":start": start_ms,
            ":end": end_ms,
            ":telemetry": "evse_telemetry",
        },
        "ScanIndexForward": True,
    }

    while True:
        resp = events_table.query(**query_kwargs)
        events.extend(resp.get("Items", []))
        last_key = resp.get("LastEvaluatedKey")
        if not last_key:
            break
        query_kwargs["ExclusiveStartKey"] = last_key

    return events


# --- Aggregation ---

def compute_aggregates(events, day_start_ms, day_end_ms):
    """Compute daily aggregates from a list of telemetry events.

    Args:
        events: List of DynamoDB items, sorted by timestamp ascending.
        day_start_ms: Start of day in ms (midnight UTC).
        day_end_ms: End of day in ms (next midnight UTC).

    Returns:
        Dict with all aggregate fields.
    """
    event_count = len(events)

    if event_count == 0:
        return {
            "event_count": 0,
            "availability_pct": 0.0,
            "longest_gap_minutes": 1440,
            "total_kwh": 0.0,
            "charge_session_count": 0,
            "charge_duration_min": 0.0,
            "peak_current_ma": 0,
            "ac_compressor_hours": 0.0,
            "fault_sensor_count": 0,
            "fault_clamp_count": 0,
            "fault_interlock_count": 0,
            "selftest_failed": False,
        }

    # --- Availability ---
    availability_pct = min(event_count / EXPECTED_UPLINKS_PER_DAY * 100, 100.0)

    # --- Longest gap (including midnight boundaries) ---
    timestamps_ms = [int(e["timestamp"]) for e in events]
    gaps = []
    gaps.append(timestamps_ms[0] - day_start_ms)
    for i in range(1, len(timestamps_ms)):
        gaps.append(timestamps_ms[i] - timestamps_ms[i - 1])
    gaps.append(day_end_ms - timestamps_ms[-1])
    longest_gap_minutes = max(gaps) / 60_000.0

    # --- Accumulators ---
    total_kwh = 0.0
    charge_session_count = 0
    charge_duration_ms = 0
    peak_current_ma = 0
    ac_compressor_ms = 0
    fault_sensor_count = 0
    fault_clamp_count = 0
    fault_interlock_count = 0
    selftest_failed = False
    in_charge_session = False

    for i, event in enumerate(events):
        evse = event.get("data", {}).get("evse", {})
        ts = int(event["timestamp"])

        current_ma = int(evse.get("current_draw_ma", 0) or 0)
        pilot_state = evse.get("pilot_state", "A")
        cool_active = evse.get("thermostat_cool_active", False)

        # Peak current
        if current_ma > peak_current_ma:
            peak_current_ma = current_ma

        # Fault counts
        if evse.get("fault_sensor", False):
            fault_sensor_count += 1
        if evse.get("fault_clamp_mismatch", False):
            fault_clamp_count += 1
        if evse.get("fault_interlock", False):
            fault_interlock_count += 1
        if evse.get("fault_selftest_fail", False):
            selftest_failed = True

        # Time-weighted: duration from this event to the next (or to day end)
        if i < len(events) - 1:
            next_ts = int(events[i + 1]["timestamp"])
        else:
            next_ts = day_end_ms
        duration_ms = next_ts - ts

        # Energy: only during State C with measured current
        is_charging = (pilot_state == "C")
        if is_charging and current_ma > 0:
            current_a = current_ma / 1000.0
            power_w = current_a * assumed_voltage_v
            duration_h = duration_ms / 3_600_000.0
            total_kwh += power_w * duration_h / 1000.0

        # Charge session tracking
        if is_charging and not in_charge_session:
            charge_session_count += 1
            in_charge_session = True
        elif not is_charging:
            in_charge_session = False

        if is_charging:
            charge_duration_ms += duration_ms

        # AC compressor
        if cool_active:
            ac_compressor_ms += duration_ms

    return {
        "event_count": event_count,
        "availability_pct": round(availability_pct, 1),
        "longest_gap_minutes": round(longest_gap_minutes, 1),
        "total_kwh": round(total_kwh, 3),
        "charge_session_count": charge_session_count,
        "charge_duration_min": round(charge_duration_ms / 60_000.0, 1),
        "peak_current_ma": peak_current_ma,
        "ac_compressor_hours": round(ac_compressor_ms / 3_600_000.0, 2),
        "fault_sensor_count": fault_sensor_count,
        "fault_clamp_count": fault_clamp_count,
        "fault_interlock_count": fault_interlock_count,
        "selftest_failed": selftest_failed,
    }


# --- Write aggregate ---

def write_aggregate(device_id, wireless_device_id, date_str, aggregates):
    """Write a daily aggregate record to DynamoDB.

    Args:
        device_id: SC-XXXXXXXX short device ID (partition key).
        wireless_device_id: AWS IoT Wireless UUID (for reference).
        date_str: "YYYY-MM-DD" (sort key).
        aggregates: Dict from compute_aggregates().
    """
    day_dt = datetime.strptime(date_str, "%Y-%m-%d").replace(tzinfo=timezone.utc)
    day_epoch = int(day_dt.timestamp())
    ttl = day_epoch + THREE_YEAR_TTL_S

    item = {
        "device_id": device_id,
        "date": date_str,
        "wireless_device_id": wireless_device_id,
        "ttl": ttl,
        **aggregates,
    }

    aggregates_table.put_item(Item=item)


# --- Per-device orchestration ---

def aggregate_device_day(device, date_str):
    """Aggregate one device's data for one day.

    Args:
        device: Registry record with device_id, wireless_device_id.
        date_str: "YYYY-MM-DD" date to aggregate.

    Returns:
        The aggregates dict, or None if device has no wireless_device_id.
    """
    device_id = device.get("device_id", "unknown")
    wireless_id = device.get("wireless_device_id", "")
    if not wireless_id:
        print(f"Skipping {device_id}: no wireless_device_id")
        return None

    day_dt = datetime.strptime(date_str, "%Y-%m-%d").replace(tzinfo=timezone.utc)
    day_start_ms = int(day_dt.timestamp() * 1000)
    day_end_ms = day_start_ms + 86_400_000

    events = query_device_events(wireless_id, day_start_ms, day_end_ms)
    aggregates = compute_aggregates(events, day_start_ms, day_end_ms)

    write_aggregate(device_id, wireless_id, date_str, aggregates)

    return aggregates


# --- Handler ---

def lambda_handler(event, context):
    """Daily aggregation entry point."""
    print(f"Aggregation Lambda invoked: {json.dumps(event)}")

    date_str = event.get("date")
    if not date_str:
        yesterday = datetime.now(timezone.utc) - timedelta(days=1)
        date_str = yesterday.strftime("%Y-%m-%d")

    print(f"Aggregating for date: {date_str}")

    devices = get_all_active_devices()
    print(f"Found {len(devices)} active device(s)")

    if not devices:
        return {"statusCode": 200, "body": "No active devices"}

    results = {"date": date_str, "devices": 0, "errors": 0}

    for device in devices:
        try:
            agg = aggregate_device_day(device, date_str)
            if agg is not None:
                results["devices"] += 1
                print(
                    f"  {device.get('device_id')}: "
                    f"{agg['event_count']} events, "
                    f"{agg['total_kwh']} kWh, "
                    f"availability {agg['availability_pct']}%"
                )
        except Exception as e:
            results["errors"] += 1
            print(f"  {device.get('device_id', '?')}: ERROR: {e}")

    print(
        f"Aggregation complete: "
        f"{results['devices']} device(s), {results['errors']} error(s)"
    )

    return {"statusCode": 200, "body": json.dumps(results)}
