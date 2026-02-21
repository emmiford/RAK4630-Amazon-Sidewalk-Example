"""
OTA deployment — S3 upload, session management, baseline, monitoring.

Used by firmware.py CLI. Can also be imported directly.
"""

import os
import subprocess
import sys
import time
import warnings

warnings.filterwarnings("ignore")

# --- Constants ---
DEVICE_ID = "b319d001-6b08-4d88-b4ca-4d2d98a6d43c"
TABLE_NAME = "sidewalk-v1-device_events_v2"
OTA_BUCKET = "evse-ota-firmware-dev"
BASELINE_KEY = "ota/baseline.bin"
CHUNK_DATA_SIZE = 15  # 15B data + 4B header = 19B LoRa MTU

PRIMARY_ADDR = 0x90000
PRIMARY_SIZE = 256 * 1024  # 256KB primary partition

PYOCD = "/Users/emilyf/sidewalk-env/bin/pyocd"
BUILD_APP_DIR = "build_app"
APP_BIN = os.path.join(BUILD_APP_DIR, "app.bin")

sys.path.insert(0, os.path.dirname(__file__))
from protocol_constants import crc32


# --- Delta computation ---


def compute_delta_chunks(baseline, firmware, chunk_size):
    """Compare two firmware binaries chunk-by-chunk, return list of changed chunk indices."""
    fw_chunks = (len(firmware) + chunk_size - 1) // chunk_size
    changed = []
    for i in range(fw_chunks):
        offset = i * chunk_size
        new_chunk = firmware[offset : offset + chunk_size]
        old_chunk = (
            baseline[offset : offset + chunk_size]
            if offset < len(baseline)
            else b""
        )
        if len(old_chunk) < len(new_chunk):
            old_chunk = old_chunk + b"\xff" * (len(new_chunk) - len(old_chunk))
        if new_chunk != old_chunk:
            changed.append(i)
    return changed


# --- pyOCD helpers ---


def pyocd_dump(addr, size, out_path):
    """Dump flash memory via pyOCD savemem."""
    print(f"Reading 0x{addr:x} ({size} bytes) via pyOCD...")
    subprocess.run(
        [PYOCD, "cmd", "-t", "nrf52840", "-c",
         f"savemem 0x{addr:x} {size} {out_path}"],
        check=True,
    )
    with open(out_path, "rb") as f:
        data = f.read()
    # Trim trailing 0xFF (erased flash)
    end = len(data)
    while end > 0 and data[end - 1] == 0xFF:
        end -= 1
    trimmed = data[:end]
    print(f"  Read {len(data)} bytes, {len(trimmed)} bytes non-erased")

    # Warn if dump is significantly larger than the app binary
    bin_path = os.path.join(os.path.dirname(__file__), "..", BUILD_APP_DIR, "app.bin")
    if os.path.exists(bin_path):
        expected = os.path.getsize(bin_path)
        if expected > 0 and len(trimmed) > expected * 2:
            print(f"  WARNING: Dump ({len(trimmed)}B) >> app binary ({expected}B)")
            print(f"  Stale flash data likely present. Re-flash with flash.sh app first.")

    return trimmed


def pyocd_read_primary():
    """Read app primary partition from device."""
    tmp = "/tmp/ota_primary_dump.bin"
    return pyocd_dump(PRIMARY_ADDR, PRIMARY_SIZE, tmp)


# --- S3 / DynamoDB helpers ---


def get_boto3():
    import boto3
    return boto3


def get_s3():
    return get_boto3().client("s3")


def get_table():
    dynamodb = get_boto3().resource("dynamodb")
    return dynamodb.Table(TABLE_NAME)


def s3_download(key):
    """Download a file from the OTA bucket, return bytes."""
    s3 = get_s3()
    resp = s3.get_object(Bucket=OTA_BUCKET, Key=key)
    return resp["Body"].read()


def s3_upload(key, data, metadata=None):
    """Upload bytes to the OTA bucket."""
    s3 = get_s3()
    kwargs = {"Bucket": OTA_BUCKET, "Key": key, "Body": data}
    if metadata:
        kwargs["Metadata"] = metadata
    s3.put_object(**kwargs)


def get_session():
    table = get_table()
    resp = table.get_item(Key={"device_id": DEVICE_ID, "timestamp": -1})
    return resp.get("Item")


def clear_session():
    table = get_table()
    try:
        table.delete_item(Key={"device_id": DEVICE_ID, "timestamp": -1})
    except Exception as e:
        print(f"clear_session error: {e}")


def send_abort():
    """Send OTA_ABORT via IoT Wireless."""
    import base64
    import struct

    iot = get_boto3().client("iotwireless")
    payload = struct.pack("<BB", 0x20, 0x03)  # OTA_CMD_TYPE, OTA_SUB_ABORT
    b64 = base64.b64encode(payload).decode()
    iot.send_data_to_wireless_device(
        Id=DEVICE_ID,
        TransmitMode=1,
        PayloadData=b64,
        WirelessMetadata={
            "Sidewalk": {"MessageType": "CUSTOM_COMMAND_ID_NOTIFY"}
        },
    )
    print("Sent OTA_ABORT to device")


# --- Status display ---


def format_duration(seconds):
    if seconds < 60:
        return f"{seconds}s"
    m, s = divmod(seconds, 60)
    if m < 60:
        return f"{m}m {s}s"
    h, m = divmod(m, 60)
    return f"{h}h {m}m"


def print_status(session):
    if not session:
        print("No active OTA session.")
        return False

    status = session.get("status", "unknown")
    s3_key = session.get("s3_key", "?")
    fw_size = int(session.get("fw_size", 0))
    total = int(session.get("total_chunks", 0))
    next_chunk = int(session.get("next_chunk", 0))
    highest_acked = int(session.get("highest_acked", 0))
    chunk_size = int(session.get("chunk_size", 15))
    retries = int(session.get("retries", 0))
    version = int(session.get("version", 0))
    started_at = int(session.get("started_at", 0))
    updated_at = int(session.get("updated_at", 0))
    baseline_crc = session.get("baseline_crc32")
    baseline_size = session.get("baseline_size")
    delta_chunks_json = session.get("delta_chunks")
    now = int(time.time())

    progress_chunks = max(next_chunk, highest_acked)
    pct = (progress_chunks / total * 100) if total else 0
    bytes_done = min(progress_chunks * chunk_size, fw_size)

    elapsed = now - started_at if started_at else 0
    stale_secs = now - updated_at if updated_at else 0
    eta_str = "?"
    if progress_chunks > 0 and elapsed > 0:
        rate = elapsed / progress_chunks
        remaining = total - progress_chunks
        eta_str = format_duration(int(remaining * rate))

    bar_width = 30
    filled = int(bar_width * pct / 100)
    bar = "#" * filled + "-" * (bar_width - filled)

    mode = "delta" if delta_chunks_json else "full"

    print(f"OTA v{version}: {s3_key}  [{mode}]")
    print(f"  Status:   {status} (retries: {retries})")
    print(f"  Progress: [{bar}] {pct:.1f}%")
    print(f"  Chunks:   {progress_chunks} / {total}  ({bytes_done} / {fw_size} bytes)")
    print(f"  Elapsed:  {format_duration(elapsed)}  |  Last activity: {stale_secs}s ago")
    print(f"  ETA:      {eta_str}")
    if baseline_crc:
        bl_info = f"0x{int(baseline_crc):08x}"
        if baseline_size:
            bl_info += f" ({int(baseline_size)}B)"
        print(f"  Baseline: {bl_info}")

    return status not in ("complete",)
