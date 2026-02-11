#!/usr/bin/env python3
"""
OTA Deploy — single CLI for building, uploading, and monitoring firmware OTA updates.

Usage:
    python aws/ota_deploy.py baseline              # Dump device primary → S3 baseline
    python aws/ota_deploy.py deploy [--build] [--version N] [--remote] [--force]
    python aws/ota_deploy.py preview               # Show delta without deploying
    python aws/ota_deploy.py status [--watch [N]]  # Monitor OTA progress
    python aws/ota_deploy.py abort                 # Send OTA_ABORT + clear session
    python aws/ota_deploy.py clear-session         # Clear DynamoDB session only
"""

import argparse
import binascii
import json
import os
import re
import subprocess
import sys
import time
import warnings

warnings.filterwarnings("ignore")

# --- Constants ---
DEVICE_ID = "b319d001-6b08-4d88-b4ca-4d2d98a6d43c"
TABLE_NAME = "sidewalk-v1-device_events_v2"
OTA_BUCKET = "evse-ota-firmware-dev"
BASELINE_KEY = "firmware/baseline.bin"
CHUNK_DATA_SIZE = 15  # 15B data + 4B header = 19B LoRa MTU

PRIMARY_ADDR = 0x90000
PRIMARY_SIZE = 256 * 1024  # 256KB primary partition

PYOCD = "/Users/emilyf/sidewalk-env/bin/pyocd"
BUILD_APP_DIR = "build_app"
APP_BIN = os.path.join(BUILD_APP_DIR, "app.bin")
APP_TX_PATH = "rak-sid/app/rak4631_evse_monitor/src/app_evse/app_tx.c"

NRFUTIL_PREFIX = (
    "nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c"
)


def crc32(data):
    return binascii.crc32(data) & 0xFFFFFFFF


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
    return trimmed


def pyocd_read_primary():
    """Read app primary partition from device."""
    tmp = "/tmp/ota_primary_dump.bin"
    return pyocd_dump(PRIMARY_ADDR, PRIMARY_SIZE, tmp)


# --- Source version helpers ---


def get_source_version():
    """Read EVSE_VERSION from app_tx.c."""
    with open(APP_TX_PATH, "r") as f:
        content = f.read()
    m = re.search(r"#define\s+EVSE_VERSION\s+(0x[0-9a-fA-F]+|\d+)", content)
    if not m:
        return None
    return int(m.group(1), 0)


def patch_version(version):
    """Write EVSE_VERSION in app_tx.c."""
    with open(APP_TX_PATH, "r") as f:
        content = f.read()
    new_content = re.sub(
        r"(#define\s+EVSE_VERSION\s+)(?:0x[0-9a-fA-F]+|\d+)",
        rf"\g<1>0x{version:02x}",
        content,
    )
    if new_content == content:
        cur = get_source_version()
        if cur == version:
            print(f"EVSE_VERSION already 0x{version:02x}, no change needed")
            return True
        print(f"WARNING: EVSE_VERSION not found in {APP_TX_PATH}")
        return False
    with open(APP_TX_PATH, "w") as f:
        f.write(new_content)
    print(f"Patched EVSE_VERSION → 0x{version:02x}")
    return True


# --- Build helper ---


def build_app():
    """Build the EVSE app via nrfutil toolchain-manager."""
    print("Building app...")
    build_cmd = (
        f"rm -rf {BUILD_APP_DIR} && mkdir {BUILD_APP_DIR} && "
        f"cd {BUILD_APP_DIR} && "
        f"cmake ../rak-sid/app/rak4631_evse_monitor/app_evse && make"
    )
    result = subprocess.run(
        f'{NRFUTIL_PREFIX} "{build_cmd}"',
        shell=True,
        cwd="/Users/emilyf/sidewalk-projects",
    )
    if result.returncode != 0:
        print("ERROR: Build failed")
        sys.exit(1)
    bin_path = os.path.join("/Users/emilyf/sidewalk-projects", APP_BIN)
    size = os.path.getsize(bin_path)
    print(f"Build OK: {bin_path} ({size} bytes)")
    return bin_path


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


def s3_upload(key, data):
    """Upload bytes to the OTA bucket."""
    s3 = get_s3()
    s3.put_object(Bucket=OTA_BUCKET, Key=key, Body=data)


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


# --- Status display (reused from ota_status.py) ---


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


# --- Subcommands ---


def cmd_baseline(args):
    """Dump device primary partition to S3 as baseline."""
    data = pyocd_read_primary()
    if not data:
        print("ERROR: Primary partition is empty (all 0xFF)")
        sys.exit(1)
    data_crc = crc32(data)
    print(f"Primary: {len(data)} bytes, CRC32=0x{data_crc:08x}")

    s3_upload(BASELINE_KEY, data)
    print(f"Uploaded to s3://{OTA_BUCKET}/{BASELINE_KEY}")

    # Also save locally for reference
    local_path = "/tmp/ota_baseline.bin"
    with open(local_path, "wb") as f:
        f.write(data)
    print(f"Local copy: {local_path}")


def cmd_deploy(args):
    """Build, verify baseline, preview delta, upload, and monitor."""
    # Step 1: Optionally patch version
    if args.version is not None:
        patch_version(args.version)

    # Step 2: Optionally build
    abs_bin = os.path.join("/Users/emilyf/sidewalk-projects", APP_BIN)
    if args.build:
        build_app()
    elif not os.path.exists(abs_bin):
        print(f"ERROR: {abs_bin} not found. Use --build to build first.")
        sys.exit(1)

    with open(abs_bin, "rb") as f:
        firmware = f.read()
    fw_crc = crc32(firmware)
    fw_version = get_source_version() or 0
    print(f"\nFirmware: {len(firmware)} bytes, CRC32=0x{fw_crc:08x}, version=0x{fw_version:02x}")

    # Step 3: Check for existing session
    session = get_session()
    if session and not args.force:
        print(f"\nWARNING: Active OTA session exists (status: {session.get('status')})")
        print("Use --force to override, or 'abort'/'clear-session' first.")
        sys.exit(1)
    elif session and args.force:
        print("Clearing existing session (--force)")
        clear_session()

    # Step 4: Download S3 baseline and verify against device
    print("\nDownloading S3 baseline...")
    try:
        baseline = s3_download(BASELINE_KEY)
        bl_crc = crc32(baseline)
        print(f"S3 baseline: {len(baseline)} bytes, CRC32=0x{bl_crc:08x}")
    except Exception as e:
        print(f"No S3 baseline found ({e})")
        print("Run 'baseline' command first to capture device state.")
        sys.exit(1)

    if not args.remote:
        print("\nVerifying S3 baseline matches device primary...")
        device_data = pyocd_read_primary()
        device_crc = crc32(device_data)
        if device_crc != bl_crc or len(device_data) != len(baseline):
            print(f"ERROR: Baseline mismatch!")
            print(f"  S3:     {len(baseline)} bytes, CRC32=0x{bl_crc:08x}")
            print(f"  Device: {len(device_data)} bytes, CRC32=0x{device_crc:08x}")
            print("Run 'baseline' to re-capture, or use --remote to skip verification.")
            sys.exit(1)
        print(f"  Match confirmed: CRC32=0x{device_crc:08x}")
    else:
        print("Skipping device verification (--remote)")

    # Step 5: Preview delta
    changed = compute_delta_chunks(baseline, firmware, CHUNK_DATA_SIZE)
    full_chunks = (len(firmware) + CHUNK_DATA_SIZE - 1) // CHUNK_DATA_SIZE
    est_time = len(changed) * 15  # ~15s per LoRa downlink

    print(f"\nDelta preview:")
    print(f"  Changed: {len(changed)}/{full_chunks} chunks")
    print(f"  Indices: {changed}")
    print(f"  Est. transfer: ~{format_duration(est_time)}")

    if not changed:
        print("\nNo changes detected — firmware matches baseline.")
        sys.exit(0)

    # Step 6: Upload to S3
    if args.version is not None:
        s3_key = f"firmware/app-v{args.version}.bin"
    else:
        s3_key = f"firmware/app-v{fw_version}.bin"
    print(f"\nUploading to s3://{OTA_BUCKET}/{s3_key} ...")
    s3_upload(s3_key, firmware)
    print("Upload complete — Lambda triggered")

    # Step 7: Monitor progress
    print("\nMonitoring OTA progress (Ctrl-C to stop)...\n")
    session_seen = False
    try:
        while True:
            time.sleep(5)
            session = get_session()
            print(f"\033[2J\033[H", end="")  # clear screen
            print("OTA Deploy Monitor (Ctrl-C to stop)\n")
            if session:
                session_seen = True
                status = session.get("status", "unknown")
                print_status(session)
                if status in ("complete",):
                    print("\nOTA complete!")
                    break
            elif session_seen:
                print("Session cleared (aborted or completed).")
                break
            else:
                print("Waiting for session to start...")
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped. Use 'status' to check later.")


def cmd_preview(args):
    """Show delta between local build and S3 baseline without deploying."""
    abs_bin = os.path.join("/Users/emilyf/sidewalk-projects", APP_BIN)
    if not os.path.exists(abs_bin):
        print(f"ERROR: {abs_bin} not found. Build first.")
        sys.exit(1)

    with open(abs_bin, "rb") as f:
        firmware = f.read()
    fw_crc = crc32(firmware)
    print(f"Firmware: {len(firmware)} bytes, CRC32=0x{fw_crc:08x}")

    print("Downloading S3 baseline...")
    try:
        baseline = s3_download(BASELINE_KEY)
        bl_crc = crc32(baseline)
        print(f"Baseline: {len(baseline)} bytes, CRC32=0x{bl_crc:08x}")
    except Exception as e:
        print(f"ERROR: No S3 baseline ({e}). Run 'baseline' first.")
        sys.exit(1)

    changed = compute_delta_chunks(baseline, firmware, CHUNK_DATA_SIZE)
    full_chunks = (len(firmware) + CHUNK_DATA_SIZE - 1) // CHUNK_DATA_SIZE
    est_time = len(changed) * 15

    print(f"\nDelta: {len(changed)}/{full_chunks} chunks changed")
    print(f"Indices: {changed}")
    print(f"Est. transfer: ~{format_duration(est_time)}")

    if not changed:
        print("Firmware matches baseline — nothing to deploy.")
        return

    # Show byte-level diff for each changed chunk
    print()
    for idx in changed:
        offset = idx * CHUNK_DATA_SIZE
        new_chunk = firmware[offset : offset + CHUNK_DATA_SIZE]
        old_chunk = (
            baseline[offset : offset + CHUNK_DATA_SIZE]
            if offset < len(baseline)
            else b""
        )
        if len(old_chunk) < len(new_chunk):
            old_chunk = old_chunk + b"\xff" * (len(new_chunk) - len(old_chunk))
        print(f"  Chunk {idx} (offset 0x{offset:04x}):")
        print(f"    old: {old_chunk.hex()}")
        print(f"    new: {new_chunk.hex()}")


def cmd_status(args):
    """Show OTA session status."""
    if args.watch is not None:
        interval = args.watch if args.watch else 30
        try:
            while True:
                print(f"\033[2J\033[H", end="")
                print(f"OTA Status  (Ctrl-C to stop, polling every {interval}s)\n")
                session = get_session()
                active = print_status(session)
                if not active:
                    break
                time.sleep(interval)
        except KeyboardInterrupt:
            print()
    else:
        session = get_session()
        print_status(session)


def cmd_abort(args):
    """Send OTA_ABORT to device and clear session."""
    send_abort()
    time.sleep(1)
    clear_session()
    print("Session cleared.")


def cmd_clear_session(args):
    """Clear DynamoDB session without sending abort to device."""
    clear_session()
    print("Session cleared.")


# --- Main ---


def main():
    parser = argparse.ArgumentParser(
        description="OTA deploy tool for EVSE firmware updates"
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # baseline
    sub.add_parser("baseline", help="Dump device primary → S3 baseline")

    # deploy
    p_deploy = sub.add_parser("deploy", help="Build, upload, and monitor OTA")
    p_deploy.add_argument(
        "--build", action="store_true", help="Build app before deploying"
    )
    p_deploy.add_argument(
        "--version", type=int, default=None, help="Patch EVSE_VERSION before build"
    )
    p_deploy.add_argument(
        "--remote",
        action="store_true",
        help="Skip pyOCD baseline verification (device not connected)",
    )
    p_deploy.add_argument(
        "--force",
        action="store_true",
        help="Override existing OTA session",
    )

    # preview
    sub.add_parser("preview", help="Show delta without deploying")

    # status
    p_status = sub.add_parser("status", help="Monitor OTA progress")
    p_status.add_argument(
        "--watch",
        nargs="?",
        const=30,
        type=int,
        metavar="SEC",
        help="Poll every N seconds (default 30)",
    )

    # abort
    sub.add_parser("abort", help="Send OTA_ABORT + clear session")

    # clear-session
    sub.add_parser("clear-session", help="Clear DynamoDB session only")

    args = parser.parse_args()

    commands = {
        "baseline": cmd_baseline,
        "deploy": cmd_deploy,
        "preview": cmd_preview,
        "status": cmd_status,
        "abort": cmd_abort,
        "clear-session": cmd_clear_session,
    }
    commands[args.command](args)


if __name__ == "__main__":
    main()
