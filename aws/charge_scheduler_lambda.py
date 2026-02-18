"""
Demand-response charge scheduler for EVSE Sidewalk device.

Runs on an EventBridge schedule. Decides whether to pause or allow charging
based on:
  1. Xcel Colorado TOU rate schedule (on-peak: 5-9 PM MT, weekdays)
  2. WattTime MOER grid signal for PSCO region

Sends delay window downlinks [start, end] in SideCharge epoch to the device.
The device pauses autonomously during the window and resumes when it expires —
no cloud "allow" message needed for normal expiry.

Heartbeat re-send: if the last window was sent >30 min ago and peak is still
active, re-send the window (safe because device handles expiry).

When transitioning to off-peak, sends a legacy "allow" to cancel any active
window early (rather than waiting for natural expiry).
"""

import base64
import json
import os
import struct
import time
import urllib.error
import urllib.request
from datetime import datetime
from decimal import Decimal
from zoneinfo import ZoneInfo

import boto3
from sidewalk_utils import get_device_id, send_sidewalk_msg

# --- Clients (created once per container) ---
dynamodb = boto3.resource("dynamodb")

# --- Environment ---
TABLE_NAME = os.environ.get("DYNAMODB_TABLE", "sidewalk-v1-device_events_v2")
WATTTIME_USERNAME = os.environ.get("WATTTIME_USERNAME", "")
WATTTIME_PASSWORD = os.environ.get("WATTTIME_PASSWORD", "")
MOER_THRESHOLD = int(os.environ.get("MOER_THRESHOLD", "70"))

MT = ZoneInfo("America/Denver")
WATTTIME_REGION = "PSCO"  # Public Service Company of Colorado

# Module-level caches (survive warm invocations)
_watttime_token = None

# Charge control command byte
CHARGE_CONTROL_CMD = 0x10

# Delay window constants
DELAY_WINDOW_SUBTYPE = 0x02
MOER_WINDOW_DURATION_S = 1800   # 30-minute MOER pause windows
HEARTBEAT_RESEND_S = 1800       # Re-send window if last send >30 min ago
SIDECHARGE_EPOCH_OFFSET = 1767225600  # 2026-01-01T00:00:00Z as Unix timestamp

table = dynamodb.Table(TABLE_NAME)


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


def get_tou_peak_end_sc(now_mt):
    """Get SideCharge epoch of TOU peak end (9 PM MT today)."""
    peak_end = now_mt.replace(hour=21, minute=0, second=0, microsecond=0)
    return int(peak_end.timestamp()) - SIDECHARGE_EPOCH_OFFSET


# --- DynamoDB State ---

def get_last_state():
    """Read the last-sent command state (sentinel key with timestamp=0)."""
    try:
        resp = table.get_item(Key={"device_id": get_device_id(), "timestamp": 0})
        return resp.get("Item")
    except Exception as e:
        print(f"DynamoDB: get_last_state failed: {e}")
        return None


def write_state(command, reason, moer_percent, tou_peak,
                window_start_sc=None, window_end_sc=None, sent_unix=None,
                charge_now_override_until=None):
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
    if window_start_sc is not None:
        item["window_start_sc"] = window_start_sc
    if window_end_sc is not None:
        item["window_end_sc"] = window_end_sc
    if sent_unix is not None:
        item["sent_unix"] = sent_unix
    if charge_now_override_until is not None:
        item["charge_now_override_until"] = charge_now_override_until
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
    Send a legacy charge-control downlink (subtype 0x00/0x01).

    Payload: [0x10, allowed, 0x00, 0x00]
    """
    payload_bytes = bytes([CHARGE_CONTROL_CMD, 0x01 if allowed else 0x00, 0x00, 0x00])
    print(f"Sending legacy downlink: allowed={allowed}, payload={payload_bytes.hex()}")
    send_sidewalk_msg(payload_bytes, transmit_mode=1)


def send_delay_window(start_sc, end_sc):
    """
    Send a delay window downlink to the device.

    Payload (10 bytes): [0x10, 0x02, start_le_4B, end_le_4B]
    Device pauses when start <= now <= end, resumes when now > end.
    """
    payload = bytearray(10)
    payload[0] = CHARGE_CONTROL_CMD
    payload[1] = DELAY_WINDOW_SUBTYPE
    struct.pack_into("<I", payload, 2, start_sc)
    struct.pack_into("<I", payload, 6, end_sc)
    print(f"Sending delay window: start={start_sc} end={end_sc} "
          f"(duration={end_sc - start_sc}s), payload={payload.hex()}")
    send_sidewalk_msg(bytes(payload), transmit_mode=1)


# --- Handler ---

def lambda_handler(event, context):
    """EventBridge scheduled handler. Also handles divergence re-sends."""
    now_mt = datetime.now(MT)
    now_unix = int(time.time())
    now_sc = now_unix - SIDECHARGE_EPOCH_OFFSET
    force_resend = event.get("force_resend", False)
    print(f"Charge scheduler invoked at {now_mt.isoformat()}"
          f"{' (force_resend)' if force_resend else ''}")

    # 1. TOU check
    tou_peak = is_tou_peak(now_mt)
    print(f"TOU: peak={tou_peak} (weekday={now_mt.weekday()}, hour={now_mt.hour})")

    # 2. WattTime check
    moer_percent = get_moer_percent()
    moer_high = moer_percent is not None and moer_percent > MOER_THRESHOLD

    # 3. Decision
    should_pause = tou_peak or moer_high

    reason_parts = []
    if tou_peak:
        reason_parts.append("tou_peak")
    if moer_high:
        reason_parts.append(f"moer>{MOER_THRESHOLD}")
    reason = ", ".join(reason_parts) if reason_parts else "off_peak"

    print(f"Decision: {'pause' if should_pause else 'allow'} (reason: {reason})")

    # Read sentinel once (used for opt-out check, heartbeat, and off-peak cancel)
    sentinel = get_last_state()

    # Extract Charge Now opt-out (preserve across writes if still active)
    override_until = None
    if sentinel:
        raw_override = sentinel.get('charge_now_override_until')
        if raw_override is not None and int(raw_override) > now_unix:
            override_until = int(raw_override)

    # 3.5 Charge Now opt-out guard (ADR-003)
    if should_pause and override_until is not None:
        print(f"Charge Now opt-out active (until {override_until}), "
              f"suppressing pause")
        write_state("charge_now_optout", reason, moer_percent, tou_peak,
                    charge_now_override_until=override_until)
        return {"statusCode": 200,
                "body": f"suppressed: charge_now_optout ({reason})"}

    if not should_pause:
        # Off-peak: cancel any active delay window with legacy allow
        last_cmd = sentinel.get("last_command") if sentinel else None
        if last_cmd == "delay_window" or (force_resend and last_cmd == "allow"):
            label = "force re-send allow" if force_resend else \
                    "Cancelling active delay window with legacy allow"
            print(label)
            send_charge_command(True)
            write_state("allow", reason, moer_percent, tou_peak,
                        sent_unix=now_unix,
                        charge_now_override_until=override_until)
            log_command_event("allow", reason, moer_percent, tou_peak)
            return {"statusCode": 200, "body": f"sent: allow ({reason})"}

        # No window to cancel — just update sentinel
        write_state("off_peak", reason, moer_percent, tou_peak,
                    charge_now_override_until=override_until)
        return {"statusCode": 200, "body": f"off_peak ({reason})"}

    # 4. Calculate delay window end
    end_sc = now_sc
    if tou_peak:
        tou_end = get_tou_peak_end_sc(now_mt)
        end_sc = max(end_sc, tou_end)
    if moer_high:
        moer_end = now_sc + MOER_WINDOW_DURATION_S
        end_sc = max(end_sc, moer_end)

    # 5. Heartbeat check — skip if recently sent same window
    #    Bypass when force_resend is set (divergence re-send)
    if not force_resend:
        if sentinel and sentinel.get("last_command") == "delay_window":
            last_end = int(sentinel.get("window_end_sc", 0))
            last_sent = int(sentinel.get("sent_unix", 0))
            stale = (now_unix - last_sent) >= HEARTBEAT_RESEND_S
            changed = last_end != end_sc
            if not stale and not changed:
                print(f"Recently sent same window (end={end_sc}), skipping")
                write_state("delay_window", reason, moer_percent, tou_peak,
                            window_start_sc=now_sc, window_end_sc=end_sc,
                            sent_unix=last_sent,
                            charge_now_override_until=override_until)
                return {"statusCode": 200,
                        "body": f"no change: delay_window ({reason})"}

    # 6. Send delay window
    send_delay_window(now_sc, end_sc)

    # 7. Record state
    write_state("delay_window", reason, moer_percent, tou_peak,
                window_start_sc=now_sc, window_end_sc=end_sc, sent_unix=now_unix,
                charge_now_override_until=override_until)
    log_command_event("delay_window", reason, moer_percent, tou_peak)

    print(f"Delay window sent: [{now_sc}, {end_sc}] ({reason})")
    return {"statusCode": 200, "body": f"sent: delay_window ({reason})"}
