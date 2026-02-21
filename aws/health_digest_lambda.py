"""Daily health digest Lambda — summarizes fleet status and publishes to SNS.

Scans the device registry for all active devices, checks last-seen timestamps
against the heartbeat interval, queries recent fault events, and publishes a
summary email via SNS.

When AUTO_DIAG_ENABLED is true, the digest also sends a 0x40 diagnostics
request to any unhealthy device (offline too long, recent faults, or stale
firmware). Diagnostics responses received since the last digest are included
in the report.

Environment variables:
    DEVICE_REGISTRY_TABLE: DynamoDB table for device registry
    DYNAMODB_TABLE: DynamoDB table for EVSE events
    SNS_TOPIC_ARN: SNS topic for alert delivery
    HEARTBEAT_INTERVAL_S: Device heartbeat interval in seconds (default 900)
    AUTO_DIAG_ENABLED: Enable auto-diagnostics queries (default "false")
    LATEST_APP_VERSION: Latest deployed app version number (default 0 = skip check)
"""

import calendar
import json
import os
import time

import boto3

from sidewalk_utils import send_sidewalk_msg

dynamodb = boto3.resource("dynamodb")
sns = boto3.client("sns")

REGISTRY_TABLE_NAME = os.environ.get("DEVICE_REGISTRY_TABLE", "device-registry")
EVENTS_TABLE_NAME = os.environ.get("DYNAMODB_TABLE", "sidewalk-v1-device_events_v2")
SNS_TOPIC_ARN = os.environ.get("SNS_TOPIC_ARN", "")
HEARTBEAT_INTERVAL_S = int(os.environ.get("HEARTBEAT_INTERVAL_S", "900"))
AUTO_DIAG_ENABLED = os.environ.get("AUTO_DIAG_ENABLED", "false").lower() == "true"
LATEST_APP_VERSION = int(os.environ.get("LATEST_APP_VERSION", "0"))

registry_table = dynamodb.Table(REGISTRY_TABLE_NAME)
events_table = dynamodb.Table(EVENTS_TABLE_NAME)

# Device is considered offline if no uplink for 2x heartbeat
OFFLINE_THRESHOLD_MULTIPLIER = 2

# Diagnostics request command byte (TDD §4.4)
DIAG_REQUEST_CMD = 0x40


def get_all_active_devices():
    """Scan device registry for all active devices.

    Returns list of device records with device_id, wireless_device_id,
    last_seen, app_version, status.
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


def check_device_health(device, now_unix):
    """Check a single device's health status.

    Returns a dict with:
        device_id: SC-XXXXXXXX short ID
        wireless_device_id: AWS IoT Wireless device ID
        online: bool
        last_seen: ISO timestamp string
        seconds_since_seen: int (or None if never seen)
        app_version: int
        recent_faults: list of fault type strings
        unhealthy_reasons: list of reason strings (empty if healthy)
    """
    device_id = device.get("device_id", "unknown")
    wireless_id = device.get("wireless_device_id", "")
    last_seen_str = device.get("last_seen", "")
    app_version = device.get("app_version", 0)

    # Parse last_seen ISO timestamp
    seconds_since_seen = None
    online = False
    if last_seen_str:
        try:
            last_seen_unix = calendar.timegm(
                time.strptime(last_seen_str, "%Y-%m-%dT%H:%M:%SZ")
            )
            seconds_since_seen = int(now_unix - last_seen_unix)
            offline_threshold = HEARTBEAT_INTERVAL_S * OFFLINE_THRESHOLD_MULTIPLIER
            online = seconds_since_seen <= offline_threshold
        except (ValueError, OverflowError):
            pass

    # Query recent fault events (last 24h)
    recent_faults = []
    if wireless_id:
        recent_faults = get_recent_faults(wireless_id, now_unix)

    # Determine unhealthy reasons
    unhealthy_reasons = identify_unhealthy_reasons(
        online, recent_faults, app_version
    )

    return {
        "device_id": device_id,
        "wireless_device_id": wireless_id,
        "online": online,
        "last_seen": last_seen_str,
        "seconds_since_seen": seconds_since_seen,
        "app_version": app_version,
        "recent_faults": recent_faults,
        "unhealthy_reasons": unhealthy_reasons,
    }


def identify_unhealthy_reasons(online, recent_faults, app_version):
    """Determine why a device is unhealthy.

    Returns a list of reason strings. Empty list means healthy.
    """
    reasons = []
    if not online:
        reasons.append("offline")
    if recent_faults:
        reasons.append("faults")
    if LATEST_APP_VERSION > 0 and app_version < LATEST_APP_VERSION:
        reasons.append("stale_firmware")
    return reasons


def get_recent_faults(wireless_device_id, now_unix):
    """Query DynamoDB for fault events in the last 24 hours.

    Returns list of fault type strings (e.g., ['fault_sensor', 'fault_interlock']).
    """
    faults_seen = set()
    cutoff_ms = int((now_unix - 86400) * 1000)

    try:
        resp = events_table.query(
            KeyConditionExpression="#did = :did AND #ts > :cutoff",
            ExpressionAttributeNames={"#did": "device_id", "#ts": "timestamp"},
            ExpressionAttributeValues={
                ":did": wireless_device_id,
                ":cutoff": cutoff_ms,
            },
            ScanIndexForward=False,
            Limit=100,
        )

        for item in resp.get("Items", []):
            data = item.get("data", {})
            evse = data.get("evse", {})
            for fault_key in ("fault_sensor", "fault_clamp",
                              "fault_interlock", "fault_selftest"):
                if evse.get(fault_key):
                    faults_seen.add(fault_key)

    except Exception as e:
        print(f"Fault query failed for {wireless_device_id}: {e}")

    return sorted(faults_seen)


def send_diagnostic_requests(health_list):
    """Send 0x40 diagnostic requests to unhealthy devices.

    Returns list of device_ids that were queried.
    """
    queried = []
    for device in health_list:
        if not device["unhealthy_reasons"]:
            continue
        wireless_id = device.get("wireless_device_id", "")
        if not wireless_id:
            continue
        try:
            send_sidewalk_msg(
                bytes([DIAG_REQUEST_CMD]),
                wireless_device_id=wireless_id,
            )
            queried.append(device["device_id"])
            print(
                f"Sent 0x40 diag request to {device['device_id']} "
                f"({wireless_id}): {', '.join(device['unhealthy_reasons'])}"
            )
        except Exception as e:
            print(f"Failed to send diag to {device['device_id']}: {e}")
    return queried


def get_recent_diagnostics(wireless_device_id, now_unix):
    """Query DynamoDB for diagnostics responses in the last 24 hours.

    Returns the most recent diagnostics dict, or None.
    """
    cutoff_ms = int((now_unix - 86400) * 1000)

    try:
        resp = events_table.query(
            KeyConditionExpression="#did = :did AND #ts > :cutoff",
            FilterExpression="event_type = :dtype",
            ExpressionAttributeNames={"#did": "device_id", "#ts": "timestamp"},
            ExpressionAttributeValues={
                ":did": wireless_device_id,
                ":cutoff": cutoff_ms,
                ":dtype": "device_diagnostics",
            },
            ScanIndexForward=False,
            Limit=1,
        )

        items = resp.get("Items", [])
        if items:
            return items[0].get("data", {}).get("diagnostics")

    except Exception as e:
        print(f"Diagnostics query failed for {wireless_device_id}: {e}")

    return None


def build_digest(device_health_list, diag_responses=None):
    """Build a human-readable health digest from device health data.

    Args:
        device_health_list: List of device health dicts.
        diag_responses: Optional dict mapping device_id to diagnostics data.

    Returns a dict with:
        subject: email subject line
        body: formatted digest text
        total: int
        online: int
        offline: int
        faulted: int
        diag_queried: int (number of devices sent 0x40)
    """
    if diag_responses is None:
        diag_responses = {}

    total = len(device_health_list)
    online = sum(1 for d in device_health_list if d["online"])
    offline = total - online
    faulted = sum(1 for d in device_health_list if d["recent_faults"])

    # Build version distribution
    versions = {}
    for d in device_health_list:
        v = d.get("app_version", 0)
        versions[v] = versions.get(v, 0) + 1

    lines = [
        "EVSE Monitor Daily Health Digest",
        "=" * 40,
        "",
        f"Total devices: {total}",
        f"Online: {online}",
        f"Offline: {offline}",
        f"With recent faults: {faulted}",
        "",
    ]

    # Firmware versions
    if versions:
        lines.append("Firmware versions:")
        for v, count in sorted(versions.items(), reverse=True):
            lines.append(f"  v{v}: {count} device(s)")
        lines.append("")

    # Offline devices
    offline_devices = [d for d in device_health_list if not d["online"]]
    if offline_devices:
        lines.append("OFFLINE DEVICES:")
        for d in offline_devices:
            age = d.get("seconds_since_seen")
            age_str = f"{age // 3600}h {(age % 3600) // 60}m ago" if age else "never"
            lines.append(f"  {d['device_id']}: last seen {age_str}")
        lines.append("")

    # Faulted devices
    faulted_devices = [d for d in device_health_list if d["recent_faults"]]
    if faulted_devices:
        lines.append("DEVICES WITH RECENT FAULTS (24h):")
        for d in faulted_devices:
            faults = ", ".join(d["recent_faults"])
            lines.append(f"  {d['device_id']}: {faults}")
        lines.append("")

    # All clear
    if not offline_devices and not faulted_devices:
        lines.append("All devices healthy.")
        lines.append("")

    # Diagnostics responses
    if diag_responses:
        lines.append("DIAGNOSTICS RESPONSES:")
        for device_id, diag in sorted(diag_responses.items()):
            uptime_h = diag.get("uptime_seconds", 0) / 3600
            lines.append(
                f"  {device_id}: v{diag.get('app_version', '?')}, "
                f"uptime {uptime_h:.1f}h, boots {diag.get('boot_count', '?')}, "
                f"err={diag.get('last_error_name', 'none')}, "
                f"buf={diag.get('event_buffer_pending', 0)}"
            )
        lines.append("")

    subject = f"EVSE Health: {online}/{total} online"
    if faulted:
        subject += f", {faulted} faulted"

    return {
        "subject": subject,
        "body": "\n".join(lines),
        "total": total,
        "online": online,
        "offline": offline,
        "faulted": faulted,
        "diag_queried": len(diag_responses),
    }


def publish_digest(digest):
    """Publish the health digest to SNS."""
    if not SNS_TOPIC_ARN:
        print("No SNS_TOPIC_ARN configured, skipping publish")
        return

    sns.publish(
        TopicArn=SNS_TOPIC_ARN,
        Subject=digest["subject"][:100],
        Message=digest["body"],
    )
    print(f"Published digest to {SNS_TOPIC_ARN}")


def lambda_handler(event, context):
    """Daily health digest entry point."""
    print(f"Health digest invoked: {json.dumps(event)}")

    now_unix = time.time()
    devices = get_all_active_devices()
    print(f"Found {len(devices)} active device(s)")

    if not devices:
        print("No devices in registry, nothing to digest")
        return {"statusCode": 200, "body": "No devices"}

    health_list = [check_device_health(d, now_unix) for d in devices]

    # Auto-diagnostics: send 0x40 to unhealthy devices
    diag_queried = []
    if AUTO_DIAG_ENABLED:
        diag_queried = send_diagnostic_requests(health_list)
        print(f"Sent diagnostic requests to {len(diag_queried)} device(s)")
    else:
        print("Auto-diagnostics disabled (set AUTO_DIAG_ENABLED=true to enable)")

    # Collect diagnostics responses from the last 24h
    diag_responses = {}
    for device in health_list:
        wireless_id = device.get("wireless_device_id", "")
        if not wireless_id:
            continue
        diag = get_recent_diagnostics(wireless_id, now_unix)
        if diag:
            diag_responses[device["device_id"]] = diag

    digest = build_digest(health_list, diag_responses)

    print(digest["body"])
    publish_digest(digest)

    return {
        "statusCode": 200,
        "body": json.dumps({
            "total": digest["total"],
            "online": digest["online"],
            "offline": digest["offline"],
            "faulted": digest["faulted"],
            "diag_queried": len(diag_queried),
            "diag_responses": len(diag_responses),
        }),
    }
