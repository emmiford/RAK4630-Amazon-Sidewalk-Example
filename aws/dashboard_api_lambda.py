"""Fleet dashboard API Lambda — read-only REST API for EVSE fleet monitoring.

Routes:
    GET /devices          Fleet overview (device list + latest state)
    GET /devices/{id}     Device detail + event timeline
    GET /devices/{id}/daily  Daily aggregates (30 days)
    GET /ota              Fleet OTA activity (recent OTA events)

Auth: x-api-key header checked against DASHBOARD_API_KEY env var.

Environment variables:
    EVENTS_TABLE: DynamoDB table for EVSE events (default: evse-events)
    DEVICE_REGISTRY_TABLE: Device registry table (default: evse-devices)
    DEVICE_STATE_TABLE: Device state table (default: evse-device-state)
    DAILY_STATS_TABLE: Daily aggregates table (default: evse-daily-stats)
    DASHBOARD_API_KEY: Expected API key for auth
"""

import json
import os
import time
from datetime import datetime, timedelta
from decimal import Decimal
from zoneinfo import ZoneInfo

import boto3

from protocol_constants import unix_ms_to_mt

dynamodb = boto3.resource("dynamodb")
MT = ZoneInfo("America/Denver")

EVENTS_TABLE = os.environ.get("EVENTS_TABLE", "evse-events")
REGISTRY_TABLE = os.environ.get("DEVICE_REGISTRY_TABLE", "evse-devices")
STATE_TABLE = os.environ.get("DEVICE_STATE_TABLE", "evse-device-state")
DAILY_STATS_TABLE = os.environ.get("DAILY_STATS_TABLE", "evse-daily-stats")
DASHBOARD_API_KEY = os.environ.get("DASHBOARD_API_KEY", "")

events_table = dynamodb.Table(EVENTS_TABLE)
registry_table = dynamodb.Table(REGISTRY_TABLE)
state_table = dynamodb.Table(STATE_TABLE)
daily_stats_table = dynamodb.Table(DAILY_STATS_TABLE)

# Time window presets (minutes)
WINDOW_PRESETS = {"15m": 15, "30m": 30, "1h": 60, "4h": 240, "24h": 1440, "72h": 4320}
DEFAULT_WINDOW = "1h"

# Heartbeat interval for offline detection
HEARTBEAT_INTERVAL_S = 900  # 15 minutes
OFFLINE_MULTIPLIER = 2


class DecimalEncoder(json.JSONEncoder):
    """JSON encoder that converts Decimal to float."""
    def default(self, o):
        if isinstance(o, Decimal):
            return float(o)
        return super().default(o)


def json_response(status_code, body):
    """Build an API Gateway HTTP API response."""
    return {
        "statusCode": status_code,
        "headers": {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*",
        },
        "body": json.dumps(body, cls=DecimalEncoder),
    }


def check_auth(event):
    """Validate x-api-key header. Returns error response or None if OK."""
    if not DASHBOARD_API_KEY:
        return None  # No key configured = open access
    headers = event.get("headers", {})
    key = headers.get("x-api-key", "")
    if key != DASHBOARD_API_KEY:
        return json_response(401, {"error": "Invalid API key"})
    return None


# --- Route: GET /devices ---

def get_devices():
    """Fleet overview: device list with latest state."""
    # Scan registry
    devices = []
    scan_kwargs = {}
    while True:
        resp = registry_table.scan(**scan_kwargs)
        devices.extend(resp.get("Items", []))
        if not resp.get("LastEvaluatedKey"):
            break
        scan_kwargs["ExclusiveStartKey"] = resp["LastEvaluatedKey"]

    # Scan device-state for latest snapshots
    states = {}
    scan_kwargs = {}
    while True:
        resp = state_table.scan(**scan_kwargs)
        for item in resp.get("Items", []):
            states[item["device_id"]] = item
        if not resp.get("LastEvaluatedKey"):
            break
        scan_kwargs["ExclusiveStartKey"] = resp["LastEvaluatedKey"]

    now = time.time()
    result = []
    for dev in devices:
        device_id = dev.get("device_id", "")
        state = states.get(device_id, {})
        last_seen = state.get("last_seen", "")

        # Determine online status from last_seen MT timestamp
        online = False
        if last_seen:
            try:
                main_part = last_seen.rsplit(".", 1)[0]
                dt = datetime.strptime(main_part, "%Y-%m-%d %H:%M:%S")
                dt = dt.replace(tzinfo=MT)
                age_s = now - dt.timestamp()
                online = age_s < (HEARTBEAT_INTERVAL_S * OFFLINE_MULTIPLIER)
            except (ValueError, IndexError):
                pass

        result.append({
            "device_id": device_id,
            "status": dev.get("status", "unknown"),
            "wireless_device_id": dev.get("wireless_device_id", ""),
            "online": online,
            "last_seen": last_seen,
            "j1772_state": state.get("j1772_state", ""),
            "current_draw_ma": state.get("current_draw_ma", 0),
            "charge_allowed": state.get("charge_allowed"),
            "rssi": state.get("rssi"),
            "link_type": state.get("link_type", ""),
        })

    return json_response(200, {"devices": result, "count": len(result)})


# --- Route: GET /devices/{id} ---

def get_device_detail(device_id, window="1h", event_type=None):
    """Device detail with event timeline."""
    # Get device state
    try:
        state_resp = state_table.get_item(Key={"device_id": device_id})
        state = state_resp.get("Item", {})
    except Exception:
        state = {}

    # Get registry info
    try:
        reg_resp = registry_table.get_item(Key={"device_id": device_id})
        registry = reg_resp.get("Item", {})
    except Exception:
        registry = {}

    if not registry and not state:
        return json_response(404, {"error": f"Device {device_id} not found"})

    # Query events for the time window
    minutes = WINDOW_PRESETS.get(window, 60)
    now_ms = int(time.time() * 1000)
    start_ms = now_ms - (minutes * 60 * 1000)
    start_mt = unix_ms_to_mt(start_ms)
    end_mt = unix_ms_to_mt(now_ms)

    query_kwargs = {
        "KeyConditionExpression": "#did = :did AND #ts BETWEEN :start AND :end",
        "ExpressionAttributeNames": {
            "#did": "device_id",
            "#ts": "timestamp_mt",
        },
        "ExpressionAttributeValues": {
            ":did": device_id,
            ":start": start_mt,
            ":end": end_mt,
        },
        "ScanIndexForward": False,  # newest first
    }

    if event_type:
        query_kwargs["FilterExpression"] = "#et = :et"
        query_kwargs["ExpressionAttributeNames"]["#et"] = "event_type"
        query_kwargs["ExpressionAttributeValues"][":et"] = event_type

    events = []
    while True:
        resp = events_table.query(**query_kwargs)
        events.extend(resp.get("Items", []))
        if not resp.get("LastEvaluatedKey"):
            break
        query_kwargs["ExclusiveStartKey"] = resp["LastEvaluatedKey"]

    # Build event summaries
    summaries = [summarize_event(e) for e in events]

    return json_response(200, {
        "device_id": device_id,
        "registry": registry,
        "state": state,
        "events": summaries,
        "event_count": len(summaries),
        "window": window,
    })


def summarize_event(event):
    """Build a one-line human-readable summary for an event."""
    event_type = event.get("event_type", "unknown")
    ts = event.get("timestamp_mt", "")
    data = event.get("data", {})

    summary = ts
    anomaly = False

    if event_type == "evse_telemetry":
        evse = data.get("evse", {})
        state = evse.get("pilot_state", "?")
        current = evse.get("current_draw_ma", 0)
        voltage = evse.get("pilot_voltage_mv", 0)
        cool = evse.get("thermostat_cool_active", False)
        parts = [f"State {state}", f"{current}mA", f"{voltage}mV"]
        if cool:
            parts.append("COOL")
        # Anomaly: fault flags
        for fault in ("fault_sensor", "fault_clamp", "fault_interlock", "fault_selftest"):
            if evse.get(fault):
                parts.append(f"FAULT:{fault.split('_')[1].upper()}")
                anomaly = True
        summary = " | ".join(parts)

    elif event_type == "device_diagnostics":
        diag = data.get("diagnostics", {})
        ver = diag.get("app_version", "?")
        uptime = diag.get("uptime_seconds", 0)
        err = diag.get("last_error_name", "none")
        summary = f"v{ver} uptime={uptime}s err={err}"
        if err != "none":
            anomaly = True

    elif event_type == "scheduler_command":
        cmd = event.get("command", "?")
        reason = event.get("reason", "?")
        summary = f"{cmd} ({reason})"

    elif event_type in ("ota_start", "ota_complete", "ota_chunk"):
        fw_size = event.get("fw_size", "?")
        version = event.get("version", "?")
        summary = f"{event_type} v{version} ({fw_size}B)"

    elif event_type == "interlock_transition":
        reason = event.get("transition_reason", "?")
        allowed = event.get("charge_allowed", "?")
        summary = f"interlock: {reason} allowed={allowed}"

    return {
        "timestamp_mt": ts,
        "event_type": event_type,
        "summary": summary,
        "anomaly": anomaly,
        "raw": event,
    }


# --- Route: GET /devices/{id}/daily ---

def get_device_daily(device_id, days=30):
    """Query daily aggregates for a device."""
    end_date = datetime.now(MT).strftime("%Y-%m-%d")
    start_date = (datetime.now(MT) - timedelta(days=days)).strftime("%Y-%m-%d")

    query_kwargs = {
        "KeyConditionExpression": "#did = :did AND #d BETWEEN :start AND :end",
        "ExpressionAttributeNames": {"#did": "device_id", "#d": "date"},
        "ExpressionAttributeValues": {
            ":did": device_id,
            ":start": start_date,
            ":end": end_date,
        },
        "ScanIndexForward": False,
    }

    records = []
    while True:
        resp = daily_stats_table.query(**query_kwargs)
        records.extend(resp.get("Items", []))
        if not resp.get("LastEvaluatedKey"):
            break
        query_kwargs["ExclusiveStartKey"] = resp["LastEvaluatedKey"]

    return json_response(200, {
        "device_id": device_id,
        "records": records,
        "count": len(records),
    })


# --- Route: GET /ota ---

def get_ota_activity(limit=50):
    """Query recent OTA activity across all devices via GSI."""
    results = []

    for etype in ("ota_start", "ota_complete"):
        query_kwargs = {
            "IndexName": "event-type-index",
            "KeyConditionExpression": "#et = :et",
            "ExpressionAttributeNames": {"#et": "event_type"},
            "ExpressionAttributeValues": {":et": etype},
            "ScanIndexForward": False,
            "Limit": limit,
        }
        resp = events_table.query(**query_kwargs)
        results.extend(resp.get("Items", []))

    # Sort by timestamp descending
    results.sort(key=lambda x: x.get("timestamp_mt", ""), reverse=True)

    return json_response(200, {
        "events": [summarize_event(e) for e in results[:limit]],
        "count": min(len(results), limit),
    })


# --- Router ---

def route_request(event):
    """Route an API Gateway HTTP API event to the appropriate handler."""
    method = event.get("requestContext", {}).get("http", {}).get("method", "GET")
    path = event.get("rawPath", "/")
    params = event.get("queryStringParameters") or {}

    if method == "OPTIONS":
        return json_response(200, {})

    if method != "GET":
        return json_response(405, {"error": "Method not allowed"})

    # GET /devices
    if path == "/devices":
        return get_devices()

    # GET /devices/{id}
    if path.startswith("/devices/") and "/daily" not in path:
        device_id = path.split("/")[2]
        window = params.get("window", DEFAULT_WINDOW)
        event_type = params.get("event_type")
        return get_device_detail(device_id, window, event_type)

    # GET /devices/{id}/daily
    if path.startswith("/devices/") and path.endswith("/daily"):
        device_id = path.split("/")[2]
        days = int(params.get("days", "30"))
        return get_device_daily(device_id, days)

    # GET /ota
    if path == "/ota":
        limit = int(params.get("limit", "50"))
        return get_ota_activity(limit)

    return json_response(404, {"error": "Not found"})


# --- Handler ---

def lambda_handler(event, context):
    """Dashboard API entry point."""
    # CORS preflight must bypass auth (browsers send OPTIONS without API key)
    method = event.get("requestContext", {}).get("http", {}).get("method", "")
    if method == "OPTIONS":
        return json_response(200, {})

    auth_error = check_auth(event)
    if auth_error:
        return auth_error

    try:
        return route_request(event)
    except Exception as e:
        print(f"Dashboard API error: {e}")
        return json_response(500, {"error": str(e)})
