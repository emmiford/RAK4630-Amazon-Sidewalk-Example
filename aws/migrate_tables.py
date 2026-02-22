"""One-time migration script: old DynamoDB tables → new evse-* schema (ADR-006).

Migrates:
  sidewalk-v1-device_events_v2 → evse-events (SC-ID PK, MT timestamp SK)
  device-registry → evse-devices (unchanged schema, just renamed)
  daily-aggregates → evse-daily-stats (unchanged schema, just renamed)
  sentinel keys (timestamp 0, -1, -2, -3) → evse-device-state

Idempotent: safe to re-run. Does NOT delete old tables.

Usage:
  python3 migrate_tables.py [--dry-run]
"""

import argparse
import hashlib
import sys
import time
from datetime import datetime, timezone, timedelta
from zoneinfo import ZoneInfo

import boto3
from boto3.dynamodb.types import TypeDeserializer

dynamodb = boto3.resource("dynamodb")
MT = ZoneInfo("America/Denver")

# Old table names
OLD_EVENTS = "sidewalk-v1-device_events_v2"
OLD_REGISTRY = "device-registry"
OLD_AGGREGATES = "daily-aggregates"

# New table names
NEW_EVENTS = "evse-events"
NEW_DEVICES = "evse-devices"
NEW_DAILY_STATS = "evse-daily-stats"
NEW_DEVICE_STATE = "evse-device-state"

# Sentinel timestamp values
SENTINEL_SCHEDULER = 0
SENTINEL_OTA = -1
SENTINEL_TIME_SYNC = -2
SENTINEL_DIVERGENCE = -3


def generate_sc_id(wireless_device_id):
    """Generate SC-XXXXXXXX from UUID."""
    digest = hashlib.sha256(wireless_device_id.encode()).hexdigest()
    return f"SC-{digest[:8].upper()}"


def unix_ms_to_mt(unix_ms):
    """Convert Unix ms to Mountain Time string."""
    dt = datetime.fromtimestamp(unix_ms / 1000, tz=MT)
    return dt.strftime("%Y-%m-%d %H:%M:%S") + f".{int(unix_ms) % 1000:03d}"


def build_uuid_to_sc_map(registry_table):
    """Scan old registry to build UUID → SC-ID mapping."""
    mapping = {}
    scan_kwargs = {}
    while True:
        resp = registry_table.scan(**scan_kwargs)
        for item in resp.get("Items", []):
            sc_id = item.get("device_id", "")
            wid = item.get("wireless_device_id", "")
            if wid:
                mapping[wid] = sc_id
        last_key = resp.get("LastEvaluatedKey")
        if not last_key:
            break
        scan_kwargs["ExclusiveStartKey"] = last_key
    return mapping


def migrate_events(old_table, new_events_table, new_state_table, uuid_map, dry_run=False):
    """Migrate events table: UUID→SC-ID, Unix ms→MT string, sentinels→device-state."""
    stats = {"events": 0, "sentinels": 0, "skipped": 0, "errors": 0}
    device_states = {}  # sc_id → merged state dict

    scan_kwargs = {}
    while True:
        resp = old_table.scan(**scan_kwargs)
        items = resp.get("Items", [])

        for item in items:
            try:
                device_id = item.get("device_id", "")
                timestamp = item.get("timestamp", 0)

                # Map UUID → SC-ID
                if device_id in uuid_map:
                    sc_id = uuid_map[device_id]
                elif device_id.startswith("SC-"):
                    sc_id = device_id  # Already migrated
                else:
                    # Generate from UUID
                    sc_id = generate_sc_id(device_id)

                ts_val = int(timestamp) if timestamp else 0

                # Sentinel items → device-state
                if ts_val <= 0:
                    if sc_id not in device_states:
                        device_states[sc_id] = {"device_id": sc_id}

                    if ts_val == SENTINEL_SCHEDULER:
                        device_states[sc_id].update({
                            "scheduler_last_command": item.get("last_command"),
                            "scheduler_reason": item.get("reason"),
                            "scheduler_moer_percent": item.get("moer_percent"),
                            "scheduler_tou_peak": item.get("tou_peak"),
                            "scheduler_window_start_sc": item.get("window_start_sc"),
                            "scheduler_window_end_sc": item.get("window_end_sc"),
                            "scheduler_sent_unix": item.get("sent_unix"),
                            "scheduler_updated_at": item.get("updated_at"),
                            "charge_now_override_until": item.get("charge_now_override_until"),
                        })
                    elif ts_val == SENTINEL_OTA:
                        for k, v in item.items():
                            if k not in ("device_id", "timestamp", "event_type"):
                                device_states[sc_id][f"ota_{k}"] = v
                    elif ts_val == SENTINEL_TIME_SYNC:
                        device_states[sc_id].update({
                            "time_sync_last_unix": item.get("last_sync_unix"),
                            "time_sync_last_epoch": item.get("last_sync_epoch"),
                        })
                    elif ts_val == SENTINEL_DIVERGENCE:
                        device_states[sc_id].update({
                            "divergence_retry_count": item.get("retry_count", 0),
                            "divergence_last_unix": item.get("last_divergence_unix"),
                            "divergence_scheduler_cmd": item.get("sentinel_command"),
                            "divergence_device_allowed": item.get("device_charge_allowed"),
                        })

                    stats["sentinels"] += 1
                    continue

                # Regular event → new events table
                new_item = dict(item)
                new_item["device_id"] = sc_id
                new_item["wireless_device_id"] = device_id
                new_item["timestamp_mt"] = unix_ms_to_mt(ts_val)
                del new_item["timestamp"]

                if not dry_run:
                    new_events_table.put_item(Item=new_item)
                stats["events"] += 1

            except Exception as e:
                print(f"  Error migrating item: {e}")
                stats["errors"] += 1

        last_key = resp.get("LastEvaluatedKey")
        if not last_key:
            break
        scan_kwargs["ExclusiveStartKey"] = last_key

        # Progress
        total = stats["events"] + stats["sentinels"] + stats["skipped"]
        if total % 100 == 0:
            print(f"  Progress: {total} items processed...")

    # Write device-state items
    for sc_id, state in device_states.items():
        # Remove None values
        state = {k: v for k, v in state.items() if v is not None}
        if not dry_run:
            new_state_table.put_item(Item=state)

    print(f"  Device states written: {len(device_states)}")
    return stats


def copy_table(old_table, new_table, dry_run=False):
    """Copy all items from old table to new table (unchanged schema)."""
    count = 0
    scan_kwargs = {}
    while True:
        resp = old_table.scan(**scan_kwargs)
        for item in resp.get("Items", []):
            if not dry_run:
                new_table.put_item(Item=item)
            count += 1

        last_key = resp.get("LastEvaluatedKey")
        if not last_key:
            break
        scan_kwargs["ExclusiveStartKey"] = last_key

    return count


def main():
    parser = argparse.ArgumentParser(description="Migrate DynamoDB tables to evse-* schema")
    parser.add_argument("--dry-run", action="store_true", help="Count items without writing")
    args = parser.parse_args()

    mode = "DRY RUN" if args.dry_run else "LIVE"
    print(f"=== EVSE Table Migration ({mode}) ===\n")

    # Open tables
    old_events = dynamodb.Table(OLD_EVENTS)
    old_registry = dynamodb.Table(OLD_REGISTRY)
    old_aggregates = dynamodb.Table(OLD_AGGREGATES)
    new_events = dynamodb.Table(NEW_EVENTS)
    new_devices = dynamodb.Table(NEW_DEVICES)
    new_daily_stats = dynamodb.Table(NEW_DAILY_STATS)
    new_state = dynamodb.Table(NEW_DEVICE_STATE)

    # 1. Build UUID → SC-ID mapping
    print("1. Building UUID → SC-ID mapping from device registry...")
    uuid_map = build_uuid_to_sc_map(old_registry)
    print(f"   Found {len(uuid_map)} device(s)\n")

    # 2. Migrate events (+ extract sentinels)
    print("2. Migrating events table...")
    event_stats = migrate_events(old_events, new_events, new_state, uuid_map, args.dry_run)
    print(f"   Events: {event_stats['events']}")
    print(f"   Sentinels → device-state: {event_stats['sentinels']}")
    print(f"   Errors: {event_stats['errors']}\n")

    # 3. Copy device registry
    print("3. Copying device registry...")
    reg_count = copy_table(old_registry, new_devices, args.dry_run)
    print(f"   Copied {reg_count} device(s)\n")

    # 4. Copy daily aggregates
    print("4. Copying daily aggregates...")
    agg_count = copy_table(old_aggregates, new_daily_stats, args.dry_run)
    print(f"   Copied {agg_count} record(s)\n")

    # Summary
    print("=== Migration Summary ===")
    print(f"Events migrated: {event_stats['events']}")
    print(f"Sentinels extracted: {event_stats['sentinels']}")
    print(f"Device registry: {reg_count}")
    print(f"Daily aggregates: {agg_count}")
    print(f"Errors: {event_stats['errors']}")
    if args.dry_run:
        print("\n(DRY RUN — no data was written)")
    else:
        print("\nMigration complete. Old tables NOT deleted.")
        print("Verify new tables, then manually delete old tables.")


if __name__ == "__main__":
    main()
