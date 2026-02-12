"""
OTA Sender Lambda — orchestrates app-only firmware updates over Sidewalk.

Two trigger modes:
1. S3 PutObject: new firmware binary uploaded → start OTA session
2. Async invoke from decode Lambda: device ACK received → send next chunk

Session state tracked in DynamoDB (sentinel key timestamp=-1).
Firmware cached in /tmp for the duration of the Lambda container.
"""

import json
import os
import struct
import time
from decimal import Decimal

import boto3
from sidewalk_utils import get_device_id, send_sidewalk_msg

# --- Clients ---
dynamodb = boto3.resource("dynamodb")
s3 = boto3.client("s3")
lambda_client = boto3.client("lambda")

# --- Environment ---
TABLE_NAME = os.environ.get("DYNAMODB_TABLE", "sidewalk-v1-device_events_v2")
OTA_BUCKET = os.environ.get("OTA_BUCKET", "evse-ota-firmware-dev")
MAX_RETRIES = int(os.environ.get("OTA_MAX_RETRIES", "5"))
CHUNK_DATA_SIZE = int(os.environ.get("OTA_CHUNK_SIZE", "15"))  # 15B data + 4B header = 19B (full LoRa MTU)

table = dynamodb.Table(TABLE_NAME)

# --- Protocol constants (must match ota_update.h) ---
OTA_CMD_TYPE = 0x20
OTA_SUB_START = 0x01
OTA_SUB_CHUNK = 0x02
OTA_SUB_ABORT = 0x03

# Uplink subtypes
OTA_SUB_ACK = 0x80
OTA_SUB_COMPLETE = 0x81
OTA_SUB_STATUS = 0x82

# Status codes
OTA_STATUS_OK = 0
OTA_STATUS_CRC_ERR = 1
OTA_STATUS_FLASH_ERR = 2
OTA_STATUS_NO_SESSION = 3
OTA_STATUS_SIZE_ERR = 4

# Module-level cache
_firmware_cache = {}  # key -> bytes


def crc32(data):
    """Compute CRC32 (IEEE) matching Zephyr's crc32_ieee."""
    import binascii
    return binascii.crc32(data) & 0xFFFFFFFF


def crc16_ccitt(data, init=0xFFFF):
    """Compute CRC16-CCITT matching Zephyr's crc16_ccitt."""
    crc = init
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc


def load_firmware(bucket, key):
    """Load firmware binary, using /tmp cache."""
    cache_key = f"{bucket}/{key}"
    if cache_key in _firmware_cache:
        print(f"Firmware from cache: {cache_key}")
        return _firmware_cache[cache_key]

    tmp_path = f"/tmp/{key.replace('/', '_')}"
    try:
        with open(tmp_path, "rb") as f:
            data = f.read()
            print(f"Firmware from /tmp: {tmp_path} ({len(data)}B)")
            _firmware_cache[cache_key] = data
            return data
    except FileNotFoundError:
        pass

    print(f"Downloading firmware: s3://{bucket}/{key}")
    resp = s3.get_object(Bucket=bucket, Key=key)
    data = resp["Body"].read()
    print(f"Downloaded {len(data)} bytes")

    with open(tmp_path, "wb") as f:
        f.write(data)

    _firmware_cache[cache_key] = data
    return data


# --- Session state in DynamoDB (sentinel key: timestamp = -1) ---

def get_session():
    """Read OTA session state."""
    try:
        resp = table.get_item(Key={"device_id": get_device_id(), "timestamp": -1})
        return resp.get("Item")
    except Exception as e:
        print(f"get_session error: {e}")
        return None


def write_session(session_data):
    """Write OTA session state to sentinel key."""
    item = {
        "device_id": get_device_id(),
        "timestamp": -1,
        "event_type": "ota_session",
        **session_data,
        "updated_at": int(time.time()),
    }
    item = json.loads(json.dumps(item, default=str), parse_float=Decimal)
    table.put_item(Item=item)


def clear_session():
    """Delete OTA session state."""
    try:
        table.delete_item(Key={"device_id": get_device_id(), "timestamp": -1})
    except Exception as e:
        print(f"clear_session error: {e}")


def log_ota_event(event_type, details):
    """Log an OTA event with real timestamp."""
    item = {
        "device_id": get_device_id(),
        "timestamp": int(time.time() * 1000),
        "event_type": event_type,
        **details,
    }
    item = json.loads(json.dumps(item, default=str), parse_float=Decimal)
    table.put_item(Item=item)


# --- OTA message builders ---

def build_ota_start(total_size, total_chunks, chunk_size, fw_crc32, version):
    """Build OTA_START downlink message."""
    payload = struct.pack("<BBIHHI",
        OTA_CMD_TYPE, OTA_SUB_START,
        total_size, total_chunks, chunk_size, fw_crc32)
    payload += struct.pack("<I", version)
    return payload


def build_ota_chunk(chunk_idx, chunk_data):
    """Build OTA_CHUNK downlink message (compact 4B header for 19B LoRa limit).

    Format: cmd(1) + sub(1) + chunk_idx(2) + data(N)
    No per-chunk CRC — AEAD provides integrity, final CRC32 validates image.
    """
    header = struct.pack("<BBH",
        OTA_CMD_TYPE, OTA_SUB_CHUNK, chunk_idx)
    return header + chunk_data


def build_ota_abort():
    """Build OTA_ABORT downlink message."""
    return struct.pack("<BB", OTA_CMD_TYPE, OTA_SUB_ABORT)


# --- Handlers ---

def compute_delta_chunks(baseline, firmware, chunk_size):
    """Compare two firmware binaries chunk-by-chunk, return list of changed absolute chunk indices."""
    fw_chunks = (len(firmware) + chunk_size - 1) // chunk_size
    changed = []

    for i in range(fw_chunks):
        offset = i * chunk_size
        new_chunk = firmware[offset:offset + chunk_size]
        old_chunk = baseline[offset:offset + chunk_size] if offset < len(baseline) else b""

        # Pad shorter chunk for comparison
        if len(old_chunk) < len(new_chunk):
            old_chunk = old_chunk + b"\xff" * (len(new_chunk) - len(old_chunk))

        if new_chunk != old_chunk:
            changed.append(i)

    return changed


def handle_s3_trigger(record):
    """New firmware uploaded to S3 — start OTA session."""
    bucket = record["s3"]["bucket"]["name"]
    key = record["s3"]["object"]["key"]
    print(f"S3 trigger: s3://{bucket}/{key}")

    firmware = load_firmware(bucket, key)
    fw_size = len(firmware)
    fw_crc = crc32(firmware)

    full_chunks = (fw_size + CHUNK_DATA_SIZE - 1) // CHUNK_DATA_SIZE

    # Extract version from key if present (e.g., firmware/app-v2.bin → 2)
    version = 0
    parts = key.rsplit("-v", 1)
    if len(parts) == 2:
        try:
            version = int(parts[1].split(".")[0])
        except ValueError:
            pass

    # Check for baseline firmware to enable delta mode
    delta_chunks_list = None
    baseline_key = "firmware/baseline.bin"
    baseline_crc = None
    baseline_size = None
    try:
        baseline = load_firmware(bucket, baseline_key)
        baseline_crc = crc32(baseline)
        baseline_size = len(baseline)
        delta_chunks_list = compute_delta_chunks(baseline, firmware, CHUNK_DATA_SIZE)
        print(f"Delta mode: {len(delta_chunks_list)}/{full_chunks} chunks changed: {delta_chunks_list}")
        print(f"Baseline: {baseline_size}B, CRC32=0x{baseline_crc:08x}")
    except Exception as e:
        print(f"No baseline ({e}), using full OTA")

    # Delta mode: send fewer chunks
    if delta_chunks_list is not None and len(delta_chunks_list) < full_chunks:
        total_chunks = len(delta_chunks_list)
        mode = "delta"
    else:
        total_chunks = full_chunks
        delta_chunks_list = None
        mode = "legacy"

    print(f"OTA START: size={fw_size} chunks={total_chunks}/{full_chunks} "
          f"chunk_size={CHUNK_DATA_SIZE} crc=0x{fw_crc:08x} ver={version} "
          f"mode={mode}")

    # Save session state
    session_data = {
        "status": "starting",
        "s3_bucket": bucket,
        "s3_key": key,
        "fw_size": fw_size,
        "fw_crc32": fw_crc,
        "total_chunks": total_chunks,
        "chunk_size": CHUNK_DATA_SIZE,
        "version": version,
        "next_chunk": 0,
        "retries": 0,
        "started_at": int(time.time()),
    }
    if baseline_crc is not None:
        session_data["baseline_crc32"] = baseline_crc
        session_data["baseline_size"] = baseline_size
    if delta_chunks_list is not None:
        session_data["delta_chunks"] = json.dumps(delta_chunks_list)
        session_data["delta_cursor"] = 0
    write_session(session_data)

    log_ota_event("ota_start", {
        "s3_key": key,
        "fw_size": fw_size,
        "fw_crc32": f"0x{fw_crc:08x}",
        "total_chunks": total_chunks,
        "full_chunks": full_chunks,
        "version": version,
        "mode": mode,
        "delta_chunks": delta_chunks_list,
    })

    # Send OTA_START
    msg = build_ota_start(fw_size, total_chunks, CHUNK_DATA_SIZE, fw_crc, version)
    send_sidewalk_msg(msg)

    return {"statusCode": 200, "body": f"OTA started ({mode}): {key} ({fw_size}B, {total_chunks}/{full_chunks} chunks)"}


def handle_device_ack(ack_data):
    """Device sent ACK — send next chunk."""
    status = ack_data.get("status", 255)
    next_chunk = ack_data.get("next_chunk", 0)
    chunks_received = ack_data.get("chunks_received", 0)

    print(f"Device ACK: status={status} next={next_chunk} received={chunks_received}")

    session = get_session()
    if not session:
        print("No active OTA session, ignoring ACK")
        return {"statusCode": 200, "body": "no session"}

    if status != OTA_STATUS_OK:
        # NO_SESSION: device has no active OTA session. Re-send START so it
        # can either reply COMPLETE (firmware already applied) or begin fresh.
        if status == OTA_STATUS_NO_SESSION:
            restarts = int(session.get("restarts", 0)) + 1
            if restarts > 3:
                print("NO_SESSION: restart limit (3) exceeded, aborting")
                log_ota_event("ota_aborted", {"reason": "no_session_max_restarts"})
                clear_session()
                return {"statusCode": 200, "body": "aborted: no_session restarts"}

            print(f"NO_SESSION: re-sending OTA_START (restart {restarts}/3)")
            firmware = load_firmware(session["s3_bucket"], session["s3_key"])
            fw_crc = crc32(firmware)
            msg = build_ota_start(
                int(session["fw_size"]),
                int(session["total_chunks"]),
                int(session["chunk_size"]),
                fw_crc,
                int(session.get("version", 0))
            )
            write_session({**{k: v for k, v in session.items()
                             if k not in ("device_id", "timestamp", "updated_at")},
                           "restarts": restarts,
                           "status": "restarting"})
            send_sidewalk_msg(msg)
            return {"statusCode": 200, "body": f"no_session: resent START (restart {restarts})"}

        retries = int(session.get("retries", 0)) + 1
        if retries > MAX_RETRIES:
            print(f"Max retries ({MAX_RETRIES}) exceeded, aborting OTA")
            log_ota_event("ota_aborted", {"reason": "max_retries", "last_status": status})
            clear_session()
            send_sidewalk_msg(build_ota_abort())
            return {"statusCode": 200, "body": "aborted: max retries"}

        # Retry the chunk that failed — in delta mode, look up correct abs index
        delta_chunks_json = session.get("delta_chunks")
        if delta_chunks_json:
            delta_list = json.loads(delta_chunks_json)
            delta_cursor = int(session.get("delta_cursor", 0))
            if delta_cursor < len(delta_list):
                retry_idx = delta_list[delta_cursor]
            else:
                retry_idx = int(next_chunk)
        else:
            retry_idx = int(next_chunk)

        print(f"Retrying chunk {retry_idx} (attempt {retries})")
        write_session({**{k: v for k, v in session.items()
                         if k not in ("device_id", "timestamp", "updated_at")},
                       "next_chunk": int(next_chunk),
                       "retries": retries,
                       "status": "retrying"})

        firmware = load_firmware(session["s3_bucket"], session["s3_key"])
        send_chunk(firmware, retry_idx, int(session["chunk_size"]))
        return {"statusCode": 200, "body": f"retrying chunk {retry_idx}"}

    # Success — send next chunk (ignore stale/duplicate ACKs)
    total_chunks = int(session["total_chunks"])
    highest_acked = int(session.get("highest_acked", 0))

    if chunks_received < highest_acked:
        print(f"Stale ACK: device reports {chunks_received} received but we saw {highest_acked}, ignoring")
        return {"statusCode": 200, "body": f"stale ack {chunks_received}"}

    # Delta mode: map sequential ACK counter to absolute chunk index
    delta_chunks_json = session.get("delta_chunks")
    if delta_chunks_json:
        delta_list = json.loads(delta_chunks_json)
        delta_cursor = int(chunks_received)  # device counts received deltas sequentially

        if delta_cursor >= len(delta_list):
            print("All delta chunks acknowledged, waiting for COMPLETE")
            write_session({**{k: v for k, v in session.items()
                         if k not in ("device_id", "timestamp", "updated_at")},
                           "status": "validating",
                           "delta_cursor": delta_cursor,
                           "highest_acked": chunks_received})
            return {"statusCode": 200, "body": "all delta chunks sent, awaiting COMPLETE"}

        abs_chunk_idx = delta_list[delta_cursor]
        print(f"Delta: sending chunk {delta_cursor}/{len(delta_list)} (abs idx {abs_chunk_idx})")

        write_session({**{k: v for k, v in session.items()
                         if k not in ("device_id", "timestamp", "updated_at")},
                       "delta_cursor": delta_cursor,
                       "retries": 0,
                       "status": "sending",
                       "highest_acked": chunks_received})

        firmware = load_firmware(session["s3_bucket"], session["s3_key"])
        send_chunk(firmware, abs_chunk_idx, int(session["chunk_size"]))

        return {"statusCode": 200, "body": f"delta chunk {delta_cursor}/{len(delta_list)} (abs {abs_chunk_idx})"}

    # Full mode: next_chunk is the sequential index
    chunk_idx = int(next_chunk)

    if chunks_received == highest_acked and chunk_idx <= int(session.get("next_chunk", 0)):
        print(f"Duplicate ACK: chunk {chunk_idx} already sent (received={chunks_received}), ignoring")
        return {"statusCode": 200, "body": f"dup ack {chunk_idx}"}

    if chunk_idx >= total_chunks:
        print("All chunks acknowledged, waiting for COMPLETE")
        write_session({**{k: v for k, v in session.items()
                         if k not in ("device_id", "timestamp", "updated_at")},
                       "status": "validating",
                       "next_chunk": chunk_idx,
                       "highest_acked": chunks_received})
        return {"statusCode": 200, "body": "all chunks sent, awaiting COMPLETE"}

    write_session({**{k: v for k, v in session.items()
                     if k not in ("device_id", "timestamp", "updated_at")},
                   "next_chunk": chunk_idx,
                   "retries": 0,
                   "status": "sending",
                   "highest_acked": chunks_received})

    firmware = load_firmware(session["s3_bucket"], session["s3_key"])
    send_chunk(firmware, chunk_idx, int(session["chunk_size"]))

    return {"statusCode": 200, "body": f"sent chunk {chunk_idx}/{total_chunks}"}


def handle_device_complete(complete_data):
    """Device finished — log result and clean up session."""
    result = complete_data.get("result", 255)
    crc_calc = complete_data.get("crc32_calc", "0x00000000")

    print(f"Device COMPLETE: result={result} crc={crc_calc}")

    session = get_session()

    log_ota_event("ota_complete", {
        "result": result,
        "crc32_calc": crc_calc,
        "success": result == OTA_STATUS_OK,
    })

    if result == OTA_STATUS_OK and session:
        # Save successful firmware as baseline for future delta OTAs
        try:
            src_bucket = session.get("s3_bucket", OTA_BUCKET)
            src_key = session["s3_key"]
            s3.copy_object(
                Bucket=src_bucket,
                CopySource={"Bucket": src_bucket, "Key": src_key},
                Key="firmware/baseline.bin",
            )
            print(f"Saved baseline: s3://{src_bucket}/firmware/baseline.bin")
        except Exception as e:
            print(f"Failed to save baseline: {e}")

    clear_session()

    if result == OTA_STATUS_OK:
        print("OTA update successful! Device will reboot.")
    else:
        print(f"OTA update failed with status {result}")

    return {"statusCode": 200, "body": f"OTA complete: result={result}"}


def handle_retry_check(event):
    """EventBridge timer — check for stale sessions and retry."""
    session = get_session()
    if not session:
        return {"statusCode": 200, "body": "no active session"}

    updated_at = int(session.get("updated_at", 0))
    elapsed = int(time.time()) - updated_at

    if elapsed < 30:
        print(f"Session active ({elapsed}s ago), no retry needed")
        return {"statusCode": 200, "body": "session active"}

    retries = int(session.get("retries", 0)) + 1
    if retries > MAX_RETRIES:
        print("Session stale and max retries exceeded, aborting")
        log_ota_event("ota_aborted", {"reason": "stale_max_retries"})
        clear_session()
        return {"statusCode": 200, "body": "aborted: stale"}

    next_chunk = int(session.get("next_chunk", 0))
    status = session.get("status", "unknown")

    print(f"Session stale ({elapsed}s), retrying: status={status} chunk={next_chunk}")

    write_session({**{k: v for k, v in session.items()
                     if k not in ("device_id", "timestamp", "updated_at")},
                   "retries": retries,
                   "status": "retrying"})

    if status in ("starting", "validating", "restarting"):
        # Re-send START — covers initial start, lost COMPLETE (validating),
        # and NO_SESSION restart. Device will reply COMPLETE if already applied.
        firmware = load_firmware(session["s3_bucket"], session["s3_key"])
        fw_crc = crc32(firmware)
        msg = build_ota_start(
            int(session["fw_size"]),
            int(session["total_chunks"]),
            int(session["chunk_size"]),
            fw_crc,
            int(session.get("version", 0))
        )
        send_sidewalk_msg(msg)
    else:
        # Re-send the chunk the device is waiting for
        firmware = load_firmware(session["s3_bucket"], session["s3_key"])

        # Delta mode: look up absolute index from delta list
        delta_chunks_json = session.get("delta_chunks")
        if delta_chunks_json:
            delta_list = json.loads(delta_chunks_json)
            delta_cursor = int(session.get("delta_cursor", next_chunk))
            if delta_cursor < len(delta_list):
                abs_idx = delta_list[delta_cursor]
                print(f"Delta retry: cursor={delta_cursor} abs_idx={abs_idx}")
                send_chunk(firmware, abs_idx, int(session["chunk_size"]))
            else:
                print(f"Delta retry: cursor {delta_cursor} past end, ignoring")
        else:
            send_chunk(firmware, next_chunk, int(session["chunk_size"]))

    return {"statusCode": 200, "body": f"retried (attempt {retries})"}


def send_chunk(firmware, chunk_idx, chunk_size):
    """Extract and send a single chunk."""
    offset = chunk_idx * chunk_size
    chunk_data = firmware[offset:offset + chunk_size]

    if not chunk_data:
        print(f"WARNING: chunk {chunk_idx} is empty (offset {offset}, fw size {len(firmware)})")
        return

    msg = build_ota_chunk(chunk_idx, chunk_data)
    send_sidewalk_msg(msg)
    print(f"Sent chunk {chunk_idx}: {len(chunk_data)}B at offset {offset}")


# --- Lambda handler ---

def lambda_handler(event, context):
    """
    Main entry point. Dispatch based on event source:
    - S3 notification: start new OTA session
    - Async invoke with ota_event: handle device response
    - EventBridge: retry check
    """
    print(f"Event: {json.dumps(event, default=str)}")

    # S3 trigger
    if "Records" in event:
        for record in event["Records"]:
            if record.get("eventSource") == "aws:s3":
                return handle_s3_trigger(record)

    # Device response forwarded from decode Lambda
    if "ota_event" in event:
        ota_event = event["ota_event"]
        event_type = ota_event.get("type", "")

        if event_type == "ack":
            return handle_device_ack(ota_event)
        elif event_type == "complete":
            return handle_device_complete(ota_event)
        elif event_type == "status":
            print(f"Device STATUS: {json.dumps(ota_event)}")
            log_ota_event("ota_device_status", ota_event)
            return {"statusCode": 200, "body": "status logged"}

    # EventBridge retry timer
    if "source" in event and event["source"] == "aws.events":
        return handle_retry_check(event)

    print("Unknown event type, ignoring")
    return {"statusCode": 400, "body": "unknown event"}
