# Shared Sidewalk utilities for Lambda functions
import base64
import os

import boto3

iot_wireless = boto3.client("iotwireless")
_device_id = None

# Device registry table (used for multi-device lookup)
_dynamodb = boto3.resource("dynamodb")
_registry_table_name = os.environ.get("DEVICE_REGISTRY_TABLE", "evse-device-registry")
_registry_table = _dynamodb.Table(_registry_table_name)


def get_device_id(target_sc_id=None):
    """Auto-discover the Sidewalk device ID. Cached per container.

    Args:
        target_sc_id: Optional SC-XXXXXXXX short ID to look up a specific device.
                      If None, returns the first active device from the registry,
                      falling back to IoT Wireless list.
    """
    global _device_id

    # If targeting a specific device, look it up in the registry
    if target_sc_id is not None:
        try:
            resp = _registry_table.get_item(Key={"device_id": target_sc_id})
            if "Item" in resp:
                device_id = resp["Item"].get("wireless_device_id")
                if device_id:
                    print(f"Registry lookup {target_sc_id}: {device_id}")
                    return device_id
        except Exception as e:
            print(f"Registry lookup failed for {target_sc_id}: {e}")
        raise RuntimeError(f"Device {target_sc_id} not found in registry")

    # Default path: return cached or discover
    if _device_id is not None:
        return _device_id

    # Try registry first — find any active device
    try:
        resp = _registry_table.scan(
            FilterExpression="#s = :active",
            ExpressionAttributeNames={"#s": "status"},
            ExpressionAttributeValues={":active": "active"},
            Limit=1,
        )
        items = resp.get("Items", [])
        if items:
            _device_id = items[0].get("wireless_device_id")
            sc_id = items[0].get("device_id")
            print(f"Registry device {sc_id}: {_device_id}")
            return _device_id
    except Exception as e:
        print(f"Registry scan failed, falling back to IoT Wireless: {e}")

    # Fallback: IoT Wireless list (works even with empty registry)
    resp = iot_wireless.list_wireless_devices(
        WirelessDeviceType="Sidewalk", MaxResults=10
    )
    devices = resp.get("WirelessDeviceList", [])
    if not devices:
        raise RuntimeError("No Sidewalk devices found")

    _device_id = devices[0]["Id"]
    print(f"IoT Wireless fallback: {_device_id} ({devices[0].get('Name', 'unnamed')})")
    return _device_id


def send_sidewalk_msg(payload_bytes, transmit_mode=1, wireless_device_id=None):
    """Send a downlink message to a Sidewalk device.

    Args:
        payload_bytes: Raw payload bytes to send.
        transmit_mode: 0=best-effort, 1=reliable (default).
        wireless_device_id: Target device ID. If None, uses get_device_id().
    """
    device_id = wireless_device_id if wireless_device_id else get_device_id()
    b64 = base64.b64encode(payload_bytes).decode()
    print(f"TX: {payload_bytes.hex()} ({len(payload_bytes)}B) -> {device_id}")
    iot_wireless.send_data_to_wireless_device(
        Id=device_id,
        TransmitMode=transmit_mode,
        PayloadData=b64,
        WirelessMetadata={"Sidewalk": {"MessageType": "CUSTOM_COMMAND_ID_NOTIFY"}},
    )
