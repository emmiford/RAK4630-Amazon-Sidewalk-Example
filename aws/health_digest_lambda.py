"""Daily health digest Lambda — summarizes fleet status and publishes to SNS.

Scans the device registry for all active devices, checks last-seen timestamps
against the heartbeat interval, queries recent fault events, and publishes a
summary email via SNS.

Environment variables:
    DEVICE_REGISTRY_TABLE: DynamoDB table for device registry
    DYNAMODB_TABLE: DynamoDB table for EVSE events
    SNS_TOPIC_ARN: SNS topic for alert delivery
    HEARTBEAT_INTERVAL_S: Device heartbeat interval in seconds (default 900)
"""

import calendar
import json
import os
import time

import boto3

dynamodb = boto3.resource("dynamodb")
sns = boto3.client("sns")

registry_table_name = os.environ.get("DEVICE_REGISTRY_TABLE", "sidecharge-device-registry")
events_table_name = os.environ.get("DYNAMODB_TABLE", "sidewalk-v1-device_events_v2")
sns_topic_arn = os.environ.get("SNS_TOPIC_ARN", "")
heartbeat_interval_s = int(os.environ.get("HEARTBEAT_INTERVAL_S", "900"))

registry_table = dynamodb.Table(registry_table_name)
events_table = dynamodb.Table(events_table_name)

# Device is considered offline if no uplink for 2x heartbeat
OFFLINE_THRESHOLD_MULTIPLIER = 2


def get_all_devices():
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
        online: bool
        last_seen: ISO timestamp string
        seconds_since_seen: int (or None if never seen)
        app_version: int
        recent_faults: list of fault type strings
    """
    device_id = device.get("device_id", "unknown")
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
            offline_threshold = heartbeat_interval_s * OFFLINE_THRESHOLD_MULTIPLIER
            online = seconds_since_seen <= offline_threshold
        except (ValueError, OverflowError):
            pass

    # Query recent fault events (last 24h)
    recent_faults = []
    wireless_id = device.get("wireless_device_id", "")
    if wireless_id:
        recent_faults = get_recent_faults(wireless_id, now_unix)

    return {
        "device_id": device_id,
        "online": online,
        "last_seen": last_seen_str,
        "seconds_since_seen": seconds_since_seen,
        "app_version": app_version,
        "recent_faults": recent_faults,
    }


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
            for fault_key in ("fault_sensor", "fault_clamp_mismatch",
                              "fault_interlock", "fault_selftest_fail"):
                if evse.get(fault_key):
                    faults_seen.add(fault_key)

    except Exception as e:
        print(f"Fault query failed for {wireless_device_id}: {e}")

    return sorted(faults_seen)


def build_digest(device_health_list):
    """Build a human-readable health digest from device health data.

    Returns a dict with:
        subject: email subject line
        body: formatted digest text
        total: int
        online: int
        offline: int
        faulted: int
    """
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
        "SideCharge Daily Health Digest",
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

    subject = f"SideCharge Health: {online}/{total} online"
    if faulted:
        subject += f", {faulted} faulted"

    return {
        "subject": subject,
        "body": "\n".join(lines),
        "total": total,
        "online": online,
        "offline": offline,
        "faulted": faulted,
    }


def publish_digest(digest):
    """Publish the health digest to SNS."""
    if not sns_topic_arn:
        print("No SNS_TOPIC_ARN configured, skipping publish")
        return

    sns.publish(
        TopicArn=sns_topic_arn,
        Subject=digest["subject"][:100],
        Message=digest["body"],
    )
    print(f"Published digest to {sns_topic_arn}")


def lambda_handler(event, context):
    """Daily health digest entry point."""
    print(f"Health digest invoked: {json.dumps(event)}")

    now_unix = time.time()
    devices = get_all_devices()
    print(f"Found {len(devices)} active device(s)")

    if not devices:
        print("No devices in registry, nothing to digest")
        return {"statusCode": 200, "body": "No devices"}

    health_list = [check_device_health(d, now_unix) for d in devices]
    digest = build_digest(health_list)

    print(digest["body"])
    publish_digest(digest)

    return {
        "statusCode": 200,
        "body": json.dumps({
            "total": digest["total"],
            "online": digest["online"],
            "offline": digest["offline"],
            "faulted": digest["faulted"],
        }),
    }
