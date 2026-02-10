# Shared Sidewalk utilities for Lambda functions
import base64

import boto3

iot_wireless = boto3.client("iotwireless")
_device_id = None


def get_device_id():
    """Auto-discover the Sidewalk device ID. Cached per container."""
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
        print(f"Device: {_device_id} ({devices[0].get('Name', 'unnamed')})")
        return _device_id

    for d in devices:
        if "eric" in d.get("Name", "").lower():
            _device_id = d["Id"]
            print(f"Device 'eric': {_device_id}")
            return _device_id

    _device_id = devices[0]["Id"]
    print(f"Using first device: {_device_id}")
    return _device_id


def send_sidewalk_msg(payload_bytes, transmit_mode=1):
    """Send a downlink message to the Sidewalk device."""
    b64 = base64.b64encode(payload_bytes).decode()
    print(f"TX: {payload_bytes.hex()} ({len(payload_bytes)}B)")
    iot_wireless.send_data_to_wireless_device(
        Id=get_device_id(),
        TransmitMode=transmit_mode,
        PayloadData=b64,
        WirelessMetadata={"Sidewalk": {"MessageType": "CUSTOM_COMMAND_ID_NOTIFY"}},
    )
