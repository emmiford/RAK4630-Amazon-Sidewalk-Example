#!/usr/bin/env python3
"""
OTA Status — quick CLI to check OTA transfer progress.

Usage:
    python aws/ota_status.py              # one-shot status
    python aws/ota_status.py --watch      # poll every 30s
    python aws/ota_status.py --watch 10   # poll every 10s
"""

import argparse
import time
import warnings

warnings.filterwarnings("ignore")

import boto3

DEVICE_ID = "b319d001-6b08-4d88-b4ca-4d2d98a6d43c"
TABLE_NAME = "sidewalk-v1-device_events_v2"

dynamodb = boto3.resource("dynamodb")
table = dynamodb.Table(TABLE_NAME)


def get_session():
    resp = table.get_item(Key={"device_id": DEVICE_ID, "timestamp": -1})
    return resp.get("Item")


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
        return

    status = session.get("status", "unknown")
    s3_key = session.get("s3_key", "?")
    fw_size = int(session.get("fw_size", 0))
    total = int(session.get("total_chunks", 0))
    next_chunk = int(session.get("next_chunk", 0))
    highest_acked = int(session.get("highest_acked", 0))
    chunk_size = int(session.get("chunk_size", 12))
    retries = int(session.get("retries", 0))
    version = int(session.get("version", 0))
    started_at = int(session.get("started_at", 0))
    updated_at = int(session.get("updated_at", 0))
    now = int(time.time())

    # Progress
    progress_chunks = max(next_chunk, highest_acked)
    pct = (progress_chunks / total * 100) if total else 0
    bytes_done = min(progress_chunks * chunk_size, fw_size)

    # ETA
    elapsed = now - started_at if started_at else 0
    stale_secs = now - updated_at if updated_at else 0
    eta_str = "?"
    if progress_chunks > 0 and elapsed > 0:
        rate = elapsed / progress_chunks  # seconds per chunk
        remaining = total - progress_chunks
        eta_secs = int(remaining * rate)
        eta_str = format_duration(eta_secs)

    # Progress bar
    bar_width = 30
    filled = int(bar_width * pct / 100)
    bar = "#" * filled + "-" * (bar_width - filled)

    print(f"OTA v{version}: {s3_key}")
    print(f"  Status:   {status} (retries: {retries})")
    print(f"  Progress: [{bar}] {pct:.1f}%")
    print(f"  Chunks:   {progress_chunks} / {total}  ({bytes_done} / {fw_size} bytes)")
    print(f"  Elapsed:  {format_duration(elapsed)}  |  Last activity: {stale_secs}s ago")
    print(f"  ETA:      {eta_str}")


def main():
    parser = argparse.ArgumentParser(description="Check OTA transfer progress")
    parser.add_argument("--watch", nargs="?", const=30, type=int, metavar="SEC",
                        help="Poll every N seconds (default 30)")
    args = parser.parse_args()

    if args.watch:
        try:
            while True:
                print("\033[2J\033[H", end="")  # clear screen
                print(f"OTA Status  (Ctrl-C to stop, polling every {args.watch}s)\n")
                session = get_session()
                print_status(session)
                if session and session.get("status") in (None, "complete"):
                    break
                time.sleep(args.watch)
        except KeyboardInterrupt:
            print()
    else:
        session = get_session()
        print_status(session)


if __name__ == "__main__":
    main()
