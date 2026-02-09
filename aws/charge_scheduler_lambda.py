"""
Demand-response charge scheduler for EVSE Sidewalk device.

Runs on an EventBridge schedule. Decides whether to pause or allow charging
based on:
  1. Xcel Colorado TOU rate schedule (on-peak: 5-9 PM MT, weekdays)
  2. WattTime MOER grid signal for PSCO region

Sends downlink charge-control commands via IoT Wireless to the Sidewalk device,
and tracks last-sent state in DynamoDB to avoid redundant transmissions.
"""

import base64
import json
import os
import time
import urllib.request
import urllib.error
from datetime import datetime
from decimal import Decimal
from zoneinfo import ZoneInfo

import boto3

# --- Clients (created once per container) ---
dynamodb = boto3.resource("dynamodb")
iot_wireless = boto3.client("iotwireless")

# --- Environment ---
TABLE_NAME = os.environ.get("DYNAMODB_TABLE", "sidewalk-v1-device_events_v2")
WATTTIME_USERNAME = os.environ.get("WATTTIME_USERNAME", "")
WATTTIME_PASSWORD = os.environ.get("WATTTIME_PASSWORD", "")
MOER_THRESHOLD = int(os.environ.get("MOER_THRESHOLD", "70"))

MT = ZoneInfo("America/Denver")
WATTTIME_REGION = "PSCO"  # Public Service Company of Colorado

# Module-level caches (survive warm invocations)
_watttime_token = None
_device_id = None

# Charge control command byte
CHARGE_CONTROL_CMD = 0x10

table = dynamodb.Table(TABLE_NAME)


def get_device_id():
    """Auto-discover the Sidewalk device ID. Cached after first call."""
    global _device_id
    if _device_id is not None:
        return _device_id

    resp = iot_wireless.list_wireless_devices(
        WirelessDeviceType="Sidewalk", MaxResults=10
    )
    devices = resp.get("WirelessDeviceList", [])
    if not devices:
        raise RuntimeError("No Sidewalk devices found")
    if len(devices) == 1:
        _device_id = devices[0]["Id"]
        print(f"Auto-discovered device: {_device_id} ({devices[0].get('Name', 'unnamed')})")
        return _device_id

    # Multiple devices — pick the one named "eric" (case-insensitive)
    for d in devices:
        if "eric" in d.get("Name", "").lower():
            _device_id = d["Id"]
            print(f"Auto-discovered device 'eric': {_device_id}")
            return _device_id

    # Fallback to first device
    _device_id = devices[0]["Id"]
    print(f"Warning: multiple devices found, using first: {_device_id} ({devices[0].get('Name', 'unnamed')})")
    return _device_id


# --- WattTime API ---

def watttime_login():
    """Obtain a bearer token from WattTime using Basic auth."""
    global _watttime_token

    url = "https://api.watttime.org/login"
    credentials = f"{WATTTIME_USERNAME}:{WATTTIME_PASSWORD}"
    b64_creds = base64.b64encode(credentials.encode()).decode()

    req = urllib.request.Request(url, method="GET")
    req.add_header("Authorization", f"Basic {b64_creds}")

    with urllib.request.urlopen(req, timeout=10) as resp:
        data = json.loads(resp.read())
        _watttime_token = data["token"]
        print("WattTime: login OK")
        return _watttime_token


def get_moer_percent():
    """
    Query WattTime signal index for PSCO region.
    Returns percent (0-100) or None on failure.
    """
    global _watttime_token

    if not WATTTIME_USERNAME or not WATTTIME_PASSWORD:
        print("WattTime: credentials not configured, skipping")
        return None

    # Login if we don't have a cached token
    if _watttime_token is None:
        try:
            watttime_login()
        except Exception as e:
            print(f"WattTime: login failed: {e}")
            return None

    url = f"https://api.watttime.org/v3/signal-index?region={WATTTIME_REGION}&signal_type=co2_moer"
    req = urllib.request.Request(url)
    req.add_header("Authorization", f"Bearer {_watttime_token}")

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read())
            percent = int(data["data"][0]["value"])
            print(f"WattTime: MOER percent={percent}")
            return percent
    except urllib.error.HTTPError as e:
        if e.code == 401:
            # Token expired — re-login once
            print("WattTime: token expired, re-authenticating")
            _watttime_token = None
            try:
                watttime_login()
            except Exception as e2:
                print(f"WattTime: re-login failed: {e2}")
                return None
            # Retry the query
            req2 = urllib.request.Request(url)
            req2.add_header("Authorization", f"Bearer {_watttime_token}")
            try:
                with urllib.request.urlopen(req2, timeout=10) as resp2:
                    data = json.loads(resp2.read())
                    percent = int(data["data"][0]["value"])
                    print(f"WattTime: MOER percent={percent} (after re-auth)")
                    return percent
            except Exception as e3:
                print(f"WattTime: query failed after re-auth: {e3}")
                return None
        else:
            print(f"WattTime: query failed HTTP {e.code}: {e}")
            return None
    except Exception as e:
        print(f"WattTime: query failed: {e}")
        return None


# --- TOU Schedule ---

def is_tou_peak(now_mt):
    """Check if current time is Xcel Colorado on-peak: weekdays 5-9 PM MT."""
    return now_mt.weekday() < 5 and 17 <= now_mt.hour < 21


# --- DynamoDB State ---

def get_last_state():
    """Read the last-sent command state (sentinel key with timestamp=0)."""
    try:
        resp = table.get_item(Key={"device_id": get_device_id(), "timestamp": 0})
        return resp.get("Item")
    except Exception as e:
        print(f"DynamoDB: get_last_state failed: {e}")
        return None


def write_state(command, reason, moer_percent, tou_peak):
    """Write the current scheduler state to the sentinel key."""
    now_iso = datetime.now(MT).isoformat()
    item = {
        "device_id": get_device_id(),
        "timestamp": 0,
        "event_type": "charge_scheduler_state",
        "last_command": command,
        "reason": reason,
        "moer_percent": moer_percent if moer_percent is not None else "N/A",
        "tou_peak": tou_peak,
        "updated_at": now_iso,
    }
    item = json.loads(json.dumps(item, default=str), parse_float=Decimal)
    table.put_item(Item=item)


def log_command_event(command, reason, moer_percent, tou_peak):
    """Log each actual command sent with a real timestamp for audit."""
    now_iso = datetime.now(MT).isoformat()
    item = {
        "device_id": get_device_id(),
        "timestamp": int(time.time() * 1000),
        "event_type": "charge_scheduler_command",
        "command": command,
        "reason": reason,
        "moer_percent": moer_percent if moer_percent is not None else "N/A",
        "tou_peak": tou_peak,
        "sent_at": now_iso,
    }
    item = json.loads(json.dumps(item, default=str), parse_float=Decimal)
    table.put_item(Item=item)


# --- Downlink ---

def send_charge_command(allowed):
    """
    Send a charge-control downlink to the Sidewalk device.

    Payload: [0x10, allowed, 0x00, 0x00]
    """
    payload_bytes = bytes([CHARGE_CONTROL_CMD, 0x01 if allowed else 0x00, 0x00, 0x00])
    b64_payload = base64.b64encode(payload_bytes).decode()

    print(f"Sending downlink: allowed={allowed}, payload={payload_bytes.hex()}, b64={b64_payload}")

    iot_wireless.send_data_to_wireless_device(
        Id=get_device_id(),
        TransmitMode=0,  # 0 = no ack
        PayloadData=b64_payload,
        WirelessMetadata={
            "Sidewalk": {
                "MessageType": "CUSTOM_COMMAND_ID_NOTIFY",
            }
        },
    )


# --- Handler ---

def lambda_handler(event, context):
    """EventBridge scheduled handler."""
    now_mt = datetime.now(MT)
    print(f"Charge scheduler invoked at {now_mt.isoformat()}")

    # 1. TOU check
    tou_peak = is_tou_peak(now_mt)
    print(f"TOU: peak={tou_peak} (weekday={now_mt.weekday()}, hour={now_mt.hour})")

    # 2. WattTime check
    moer_percent = get_moer_percent()
    moer_high = moer_percent is not None and moer_percent > MOER_THRESHOLD

    # 3. Decision
    should_pause = tou_peak or moer_high
    command = "pause" if should_pause else "allow"

    reason_parts = []
    if tou_peak:
        reason_parts.append("tou_peak")
    if moer_high:
        reason_parts.append(f"moer>{MOER_THRESHOLD}")
    reason = ", ".join(reason_parts) if reason_parts else "off_peak"

    print(f"Decision: {command} (reason: {reason})")

    # 4. Check last state — skip if unchanged
    last_state = get_last_state()
    if last_state and last_state.get("last_command") == command:
        print(f"No change from last command ({command}), skipping downlink")
        # Still update the sentinel so we know we evaluated
        write_state(command, reason, moer_percent, tou_peak)
        return {"statusCode": 200, "body": f"no change: {command}"}

    # 5. Send downlink
    allowed = not should_pause
    send_charge_command(allowed)

    # 6. Record state
    write_state(command, reason, moer_percent, tou_peak)
    log_command_event(command, reason, moer_percent, tou_peak)

    print(f"Command sent: {command}")
    return {"statusCode": 200, "body": f"sent: {command} ({reason})"}
