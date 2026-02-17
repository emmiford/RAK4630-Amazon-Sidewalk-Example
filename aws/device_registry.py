"""Device registry for SideCharge — DynamoDB-backed device management.

Generates human-readable SC-XXXXXXXX short IDs from AWS wireless device IDs,
auto-provisions devices on first uplink, and tracks last-seen timestamps.
"""

import hashlib
import time


def generate_sc_short_id(wireless_device_id):
    """Generate a deterministic SC-XXXXXXXX short ID from a wireless device ID.

    Uses first 8 hex chars of SHA-256 hash.

    Args:
        wireless_device_id: AWS IoT Wireless device UUID string.

    Returns:
        String like "SC-A1B2C3D4".
    """
    digest = hashlib.sha256(wireless_device_id.encode()).hexdigest()
    return f"SC-{digest[:8].upper()}"


def get_or_create_device(table, wireless_device_id, sidewalk_id=""):
    """Look up a device by wireless_device_id, creating it if not found.

    Args:
        table: boto3 DynamoDB Table resource for the device registry.
        wireless_device_id: AWS IoT Wireless device UUID.
        sidewalk_id: Sidewalk network ID (optional, stored on create).

    Returns:
        Dict with device record (device_id, sidewalk_id, status, etc.).
    """
    sc_id = generate_sc_short_id(wireless_device_id)

    # Try to get existing device
    resp = table.get_item(Key={"device_id": sc_id})
    if "Item" in resp:
        return resp["Item"]

    # Auto-provision new device
    # Note: owner_email omitted — DynamoDB rejects empty strings for GSI keys.
    # The owner_email-index GSI is sparse: items without owner_email are excluded,
    # which is correct (unowned devices shouldn't appear in owner queries).
    now_iso = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    item = {
        "device_id": sc_id,
        "wireless_device_id": wireless_device_id,
        "sidewalk_id": sidewalk_id,
        "status": "active",
        "last_seen": now_iso,
        "app_version": 0,
        "created_at": now_iso,
    }
    table.put_item(Item=item)
    print(f"Auto-provisioned device {sc_id} (wireless={wireless_device_id})")
    return item


def update_last_seen(table, wireless_device_id, app_version=None):
    """Update a device's last_seen timestamp (and optionally app_version).

    Args:
        table: boto3 DynamoDB Table resource for the device registry.
        wireless_device_id: AWS IoT Wireless device UUID.
        app_version: Firmware version int (optional).
    """
    sc_id = generate_sc_short_id(wireless_device_id)
    now_iso = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    update_expr = "SET last_seen = :ts"
    expr_values = {":ts": now_iso}

    if app_version is not None:
        update_expr += ", app_version = :ver"
        expr_values[":ver"] = app_version

    table.update_item(
        Key={"device_id": sc_id},
        UpdateExpression=update_expr,
        ExpressionAttributeValues=expr_values,
    )
