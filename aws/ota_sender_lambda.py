"""
OTA Sender Lambda — orchestrates app-only firmware updates over Sidewalk.

Two trigger modes:
1. S3 PutObject: new firmware binary uploaded → start OTA session
2. Async invoke from decode Lambda: device ACK received → send next chunk

Session state tracked in DynamoDB (sentinel key timestamp=-1).
Firmware cached in /tmp for the duration of the Lambda container.
"""

import base64
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
WINDOW_SIZE = int(os.environ.get("OTA_WINDOW_SIZE", "26"))  # chunks per window (26 for test, 260 for production)
CHUNKS_PER_TICK = int(os.environ.get("OTA_CHUNKS_PER_TICK", "3"))  # chunks sent per EventBridge timer tick

table = dynamodb.Table(TABLE_NAME)

# --- Protocol constants (must match ota_update.h) ---
OTA_CMD_TYPE = 0x20
OTA_SUB_START = 0x01
OTA_SUB_CHUNK = 0x02
OTA_SUB_ABORT = 0x03
OTA_SUB_WINDOW_DONE = 0x04

# Uplink subtypes
OTA_SUB_ACK = 0x80
OTA_SUB_COMPLETE = 0x81
OTA_SUB_STATUS = 0x82
OTA_SUB_GAP_REPORT = 0x83
OTA_SUB_WINDOW_ACK = 0x84

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

def build_ota_start(total_size, total_chunks, chunk_size, fw_crc32, version, window_size=0):
    """Build OTA_START downlink message (19B with window_size)."""
    payload = struct.pack("<BBIHHI",
        OTA_CMD_TYPE, OTA_SUB_START,
        total_size, total_chunks, chunk_size, fw_crc32)
    payload += struct.pack("<IB", version, window_size)
    return payload  # 19 bytes


def build_ota_chunk(chunk_idx, chunk_data):
    """Build OTA_CHUNK downlink message (compact 4B header for 19B LoRa limit).

    Format: cmd(1) + sub(1) + chunk_idx(2) + data(N)
    No per-chunk CRC — AEAD provides integrity, final CRC32 validates image.
    """
    header = struct.pack("<BBH",
        OTA_CMD_TYPE, OTA_SUB_CHUNK, chunk_idx)
    return header + chunk_data


def build_window_done(window_idx):
    """Build WINDOW_DONE downlink message."""
    return struct.pack("<BBB", OTA_CMD_TYPE, OTA_SUB_WINDOW_DONE, window_idx)


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
    try:
        baseline = load_firmware(bucket, baseline_key)
        delta_chunks_list = compute_delta_chunks(baseline, firmware, CHUNK_DATA_SIZE)
        print(f"Delta mode: {len(delta_chunks_list)}/{full_chunks} chunks changed: {delta_chunks_list}")
    except Exception as e:
        print(f"No baseline ({e}), using full OTA")

    # Delta mode: send fewer chunks, no windowing needed
    if delta_chunks_list is not None and len(delta_chunks_list) < full_chunks:
        total_chunks = len(delta_chunks_list)
        window_size = 0  # Use legacy ACK mode for delta (few chunks)
        mode = "delta"
    else:
        total_chunks = full_chunks
        delta_chunks_list = None
        window_size = WINDOW_SIZE if WINDOW_SIZE > 0 else 0
        mode = "blast" if window_size else "legacy"

    print(f"OTA START: size={fw_size} chunks={total_chunks}/{full_chunks} "
          f"chunk_size={CHUNK_DATA_SIZE} crc=0x{fw_crc:08x} ver={version} "
          f"mode={mode} window={window_size}")

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
        "window_size": window_size,
    }
    if delta_chunks_list is not None:
        session_data["delta_chunks"] = json.dumps(delta_chunks_list)
        session_data["delta_cursor"] = 0
    if window_size:
        session_data.update({
            "window_start": 0,
            "blast_cursor": 0,
            "window_idx": 0,
        })
    write_session(session_data)

    log_ota_event("ota_start", {
        "s3_key": key,
        "fw_size": fw_size,
        "fw_crc32": f"0x{fw_crc:08x}",
        "total_chunks": total_chunks,
        "full_chunks": full_chunks,
        "version": version,
        "window_size": window_size,
        "mode": mode,
        "delta_chunks": delta_chunks_list,
    })

    # Send OTA_START
    msg = build_ota_start(fw_size, total_chunks, CHUNK_DATA_SIZE, fw_crc, version, window_size)
    send_sidewalk_msg(msg)

    return {"statusCode": 200, "body": f"OTA started ({mode}): {key} ({fw_size}B, {total_chunks}/{full_chunks} chunks)"}


def handle_device_ack(ack_data):
    """Device sent ACK — send next chunk (or transition to blasting in blast mode)."""
    status = ack_data.get("status", 255)
    next_chunk = ack_data.get("next_chunk", 0)
    chunks_received = ack_data.get("chunks_received", 0)

    print(f"Device ACK: status={status} next={next_chunk} received={chunks_received}")

    session = get_session()
    if not session:
        print("No active OTA session, ignoring ACK")
        return {"statusCode": 200, "body": "no session"}

    # In blast mode, the START ACK transitions us to blasting
    window_size = int(session.get("window_size", 0))
    if window_size > 0 and status == OTA_STATUS_OK:
        sess_status = session.get("status", "")
        if sess_status == "starting":
            print("Blast mode: START ACK received, transitioning to blasting")
            write_session({**_session_fields(session),
                           "status": "blasting",
                           "retries": 0})
            return {"statusCode": 200, "body": "blast mode: ready"}
        # In blast mode, ignore other per-chunk ACKs (shouldn't happen)
        print(f"Blast mode: ignoring ACK (status={sess_status})")
        return {"statusCode": 200, "body": "blast mode: ack ignored"}

    if status != OTA_STATUS_OK:
        retries = int(session.get("retries", 0)) + 1
        if retries > MAX_RETRIES:
            print(f"Max retries ({MAX_RETRIES}) exceeded, aborting OTA")
            log_ota_event("ota_aborted", {"reason": "max_retries", "last_status": status})
            clear_session()
            send_sidewalk_msg(build_ota_abort())
            return {"statusCode": 200, "body": "aborted: max retries"}

        # Retry the chunk that failed
        print(f"Retrying chunk {next_chunk} (attempt {retries})")
        write_session({**{k: v for k, v in session.items()
                         if k not in ("device_id", "timestamp", "updated_at")},
                       "next_chunk": int(next_chunk),
                       "retries": retries,
                       "status": "retrying"})

        firmware = load_firmware(session["s3_bucket"], session["s3_key"])
        send_chunk(firmware, int(next_chunk), int(session["chunk_size"]))
        return {"statusCode": 200, "body": f"retrying chunk {next_chunk}"}

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
            write_session({**_session_fields(session),
                           "status": "validating",
                           "delta_cursor": delta_cursor,
                           "highest_acked": chunks_received})
            return {"statusCode": 200, "body": "all delta chunks sent, awaiting COMPLETE"}

        abs_chunk_idx = delta_list[delta_cursor]
        print(f"Delta: sending chunk {delta_cursor}/{len(delta_list)} (abs idx {abs_chunk_idx})")

        write_session({**_session_fields(session),
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


def handle_timer_blast(session):
    """EventBridge tick in blast mode — send next batch of chunks."""
    status = session.get("status", "")
    window_size = int(session.get("window_size", 0))
    window_start = int(session.get("window_start", 0))
    blast_cursor = int(session.get("blast_cursor", 0))
    window_idx = int(session.get("window_idx", 0))
    total_chunks = int(session["total_chunks"])
    chunk_size = int(session["chunk_size"])

    if status == "awaiting_gaps":
        # Check if stale — device may have missed WINDOW_DONE
        updated_at = int(session.get("updated_at", 0))
        elapsed = int(time.time()) - updated_at
        if elapsed < 30:
            print(f"Awaiting gaps, session fresh ({elapsed}s), waiting")
            return {"statusCode": 200, "body": "awaiting gaps"}

        retries = int(session.get("retries", 0)) + 1
        if retries > MAX_RETRIES:
            print("Awaiting gaps: max retries, aborting")
            log_ota_event("ota_aborted", {"reason": "gap_max_retries"})
            clear_session()
            return {"statusCode": 200, "body": "aborted: gap retries"}

        # Re-send WINDOW_DONE
        print(f"Re-sending WINDOW_DONE (win={window_idx}, retry {retries})")
        write_session({**_session_fields(session), "retries": retries})
        send_sidewalk_msg(build_window_done(window_idx))
        return {"statusCode": 200, "body": f"resent WINDOW_DONE {window_idx}"}

    if status not in ("blasting", "starting"):
        print(f"Blast timer: unexpected status '{status}', starting blast")

    # Transition from starting to blasting on first timer tick
    if status == "starting":
        status = "blasting"

    firmware = load_firmware(session["s3_bucket"], session["s3_key"])

    # Window end (exclusive): min of window boundary and total chunks
    window_end = min(window_start + window_size, total_chunks)

    # Send up to CHUNKS_PER_TICK chunks
    sent = 0
    while blast_cursor < window_end and sent < CHUNKS_PER_TICK:
        send_chunk(firmware, blast_cursor, chunk_size)
        blast_cursor += 1
        sent += 1

    print(f"Blast: sent {sent} chunks, cursor={blast_cursor}, window=[{window_start}..{window_end})")

    if blast_cursor >= window_end:
        # Window fully sent — send WINDOW_DONE and wait for gap report / ACK
        send_sidewalk_msg(build_window_done(window_idx))
        print(f"WINDOW_DONE sent (win={window_idx})")
        write_session({**_session_fields(session),
                       "status": "awaiting_gaps",
                       "blast_cursor": blast_cursor,
                       "retries": 0})
    else:
        write_session({**_session_fields(session),
                       "status": "blasting",
                       "blast_cursor": blast_cursor})

    return {"statusCode": 200, "body": f"blast: sent {sent}, cursor={blast_cursor}"}


def handle_gap_report(gap_data):
    """Device reports missing chunks within current window — retransmit them."""
    window_idx = gap_data.get("window_idx", 0)
    gap_indices = gap_data.get("gap_indices", [])

    print(f"GAP_REPORT: win={window_idx} gaps={gap_indices}")

    session = get_session()
    if not session:
        print("No active OTA session, ignoring GAP_REPORT")
        return {"statusCode": 200, "body": "no session"}

    window_start = int(session.get("window_start", 0))
    chunk_size = int(session["chunk_size"])
    firmware = load_firmware(session["s3_bucket"], session["s3_key"])

    # Retransmit each missing chunk (absolute index = window_start + offset)
    for offset in gap_indices:
        chunk_idx = window_start + int(offset)
        send_chunk(firmware, chunk_idx, chunk_size)

    print(f"Retransmitted {len(gap_indices)} gap chunks")

    # Stay in awaiting_gaps — device may send more GAP_REPORTs or WINDOW_ACK
    write_session({**_session_fields(session),
                   "status": "awaiting_gaps",
                   "retries": 0})

    return {"statusCode": 200, "body": f"gap fill: {len(gap_indices)} chunks"}


def handle_window_ack(ack_data):
    """Device confirms window complete — advance to next window."""
    window_idx = ack_data.get("window_idx", 0)

    print(f"WINDOW_ACK: win={window_idx}")

    session = get_session()
    if not session:
        print("No active OTA session, ignoring WINDOW_ACK")
        return {"statusCode": 200, "body": "no session"}

    window_size = int(session.get("window_size", WINDOW_SIZE))
    total_chunks = int(session["total_chunks"])
    window_start = int(session.get("window_start", 0))

    # Advance window
    new_window_start = window_start + window_size
    new_window_idx = int(window_idx) + 1

    if new_window_start >= total_chunks:
        print("All windows complete, waiting for COMPLETE from device")
        write_session({**_session_fields(session),
                       "status": "validating",
                       "window_start": new_window_start,
                       "window_idx": new_window_idx})
        return {"statusCode": 200, "body": "all windows done"}

    print(f"Window {window_idx} done, advancing to window {new_window_idx} "
          f"(chunks {new_window_start}..{min(new_window_start + window_size, total_chunks)})")

    write_session({**_session_fields(session),
                   "status": "blasting",
                   "window_start": new_window_start,
                   "blast_cursor": new_window_start,
                   "window_idx": new_window_idx,
                   "retries": 0})

    return {"statusCode": 200, "body": f"advanced to window {new_window_idx}"}


def _session_fields(session):
    """Extract session fields for write_session (strip DynamoDB keys)."""
    return {k: v for k, v in session.items()
            if k not in ("device_id", "timestamp", "updated_at")}


def handle_retry_check(event):
    """EventBridge timer — check for stale sessions and retry, or drive blast mode."""
    session = get_session()
    if not session:
        return {"statusCode": 200, "body": "no active session"}

    # Blast mode: EventBridge timer drives chunk sending
    window_size = int(session.get("window_size", 0))
    status = session.get("status", "unknown")
    if window_size > 0 and status in ("blasting", "starting", "awaiting_gaps"):
        return handle_timer_blast(session)

    updated_at = int(session.get("updated_at", 0))
    elapsed = int(time.time()) - updated_at

    if elapsed < 30:
        print(f"Session active ({elapsed}s ago), no retry needed")
        return {"statusCode": 200, "body": "session active"}

    retries = int(session.get("retries", 0)) + 1
    if retries > MAX_RETRIES:
        print(f"Session stale and max retries exceeded, aborting")
        log_ota_event("ota_aborted", {"reason": "stale_max_retries"})
        clear_session()
        return {"statusCode": 200, "body": "aborted: stale"}

    next_chunk = int(session.get("next_chunk", 0))

    print(f"Session stale ({elapsed}s), retrying: status={status} chunk={next_chunk}")

    write_session({**_session_fields(session),
                   "retries": retries,
                   "status": "retrying"})

    if status == "starting":
        # Re-send START
        firmware = load_firmware(session["s3_bucket"], session["s3_key"])
        fw_crc = crc32(firmware)
        msg = build_ota_start(
            int(session["fw_size"]),
            int(session["total_chunks"]),
            int(session["chunk_size"]),
            fw_crc,
            int(session.get("version", 0)),
            window_size
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
        elif event_type == "gap_report":
            return handle_gap_report(ota_event)
        elif event_type == "window_ack":
            return handle_window_ack(ota_event)
        elif event_type == "status":
            print(f"Device STATUS: {json.dumps(ota_event)}")
            log_ota_event("ota_device_status", ota_event)
            return {"statusCode": 200, "body": "status logged"}

    # EventBridge retry timer
    if "source" in event and event["source"] == "aws.events":
        return handle_retry_check(event)

    print(f"Unknown event type, ignoring")
    return {"statusCode": 400, "body": "unknown event"}
