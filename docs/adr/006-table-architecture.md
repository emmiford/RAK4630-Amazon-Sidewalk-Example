# ADR-006: DynamoDB Table Architecture — Unified Events, SC-ID PK, Mountain Time SK

## Status
Accepted

## Context

During development, debugging device behavior requires manually querying DynamoDB, decoding nested JSON, and reconstructing timelines. Several pain points have emerged:

1. **Inconsistent table naming** — `sidewalk-v1-device_events_v2`, `device-registry`, `daily-aggregates` follow no common convention. Documentation references stale `sidecharge-*` names. Two orphan tables (`sidewalk-v1-device_events_dedupe_v2`, `sidewalk-v1-device_config`) exist in AWS.

2. **PK mismatch** — The events table uses AWS IoT Wireless UUIDs as the primary key, while the device registry uses human-readable SC-IDs (e.g., `SC-A1B2C3D4`). Cross-table queries require a UUID→SC-ID lookup.

3. **Unreadable timestamps** — Sort keys are Unix milliseconds (Number type). In the DynamoDB console, `1708425600000` requires mental conversion to understand. During incident response, this slows down debugging.

4. **Sentinel pollution** — Mutable device state (scheduler state, OTA session, time sync, divergence tracking) is stored as sentinel keys in the events table using magic timestamp values (0, -1, -2, -3). These appear in event timeline queries and must be filtered out. There's no way to get a fleet overview without scanning every device's sentinels.

5. **No fleet-wide type queries** — To find "all OTA events across all devices," you must scan every device partition. There's no GSI on event type.

## Decision

### Decision 1: Unified events table (no per-feature split)
Keep all event types (telemetry, OTA, scheduler, diagnostics, interlock) in one table (`evse-events`).

**Rationale:** The interleaved timeline is the primary debugging tool. When investigating a device issue, seeing "scheduler sent pause → device reported State B → OTA started → interlock tripped" in chronological order is essential. Splitting into separate tables would require cross-table merges to reconstruct this timeline.

**Rejected:** Separate tables per feature (more Terraform resources, more IAM policies, cross-table merges for timeline reconstruction).

### Decision 2: SC-XXXXXXXX as primary key across all tables
The events table migrates from UUID (`b319d001-...`) to SC-ID (`SC-A1B2C3D4`) as the partition key. All tables use SC-ID as PK for consistency.

**Rationale:** Human-readable, consistent PK eliminates cross-table join friction. SC-IDs are derived from the UUID via SHA-256 hash (first 4 bytes, hex-encoded), so the mapping is deterministic. The original `wireless_device_id` (UUID) is kept as a non-key attribute for IoT Wireless API calls that require it.

### Decision 3: Mountain Time string as sort key
Events table sort key changes from `timestamp` (Number, Unix ms) to `timestamp_mt` (String, format `"YYYY-MM-DD HH:MM:SS.mmm"` in America/Denver timezone).

**Rationale:** Readability in the DynamoDB console during development and debugging is the highest priority at current scale. Mountain Time is the deployment timezone — seeing `"2026-02-21 14:30:00.000"` immediately conveys "2:30 PM today" without conversion.

**DST tradeoff:** During the November fall-back hour (~1:00 AM–2:00 AM MST/MDT transition), events may sort by wall-clock order rather than true chronological order. At the 15-minute uplink rate, this affects ~2 events per year. Acceptable for an ops dashboard.

### Decision 4: GSI with PK=event_type, SK=timestamp_mt
Add a Global Secondary Index `event-type-index` with partition key `event_type` (String) and sort key `timestamp_mt` (String). Projection: ALL.

**Rationale:** Enables fleet-wide type queries ("all `ota_start` events, newest first") without iterating per-device partitions. Essential for the dashboard's fleet OTA view and anomaly detection.

**Per-device type filtering** uses the main table query (PK=device_id) with a FilterExpression on `event_type`. FilterExpression reads all items in the SK range then discards non-matches — acceptable at ~96 events/day/device (4 events/hour × 24 hours).

**Rejected:** Composite `type#timestamp` sort key — less readable, over-engineered for current scale.

### Decision 5: Sentinel extraction into device-state table
Mutable per-device state moves from sentinel keys in the events table to a dedicated `evse-device-state` table (PK=device_id, no SK).

| Sentinel | Old Location | New Location |
|----------|-------------|--------------|
| Scheduler state | timestamp=0 | `evse-device-state.scheduler_*` |
| OTA session | timestamp=-1 | `evse-device-state.ota_*` |
| Time sync | timestamp=-2 | `evse-device-state.time_sync_*` |
| Divergence tracker | timestamp=-3 | `evse-device-state.divergence_*` |

**Rationale:** Sentinels mixed with events pollute the event log. The device-state table enables O(1) fleet overview (scan one table, one item per device) instead of O(n × sentinels) reads. Single item per device, overwritten on each uplink. No TTL.

The events table becomes a pure append-only log.

### Decision 6: Consistent `evse-*` naming
All tables use `evse-` prefix, kebab-case, no version suffixes.

| Current Name | New Name |
|---|---|
| `sidewalk-v1-device_events_v2` | `evse-events` |
| `device-registry` | `evse-devices` |
| `daily-aggregates` | `evse-daily-stats` |
| *(new)* | `evse-device-state` |
| `sidewalk-v1-device_events_dedupe_v2` | **DELETE** (orphan) |
| `sidewalk-v1-device_config` | **DELETE** (orphan) |

**Rationale:** Consistent prefix groups tables in the DynamoDB console. Kebab-case matches Terraform conventions. No version suffixes — future renames use create-migrate-swap (DynamoDB doesn't support in-place rename).

## Consequences

### What becomes easier
- **Debugging:** DynamoDB console shows readable timestamps and device IDs
- **Fleet overview:** Single scan of `evse-device-state` returns all device states
- **Cross-table queries:** Same PK format everywhere, no UUID→SC-ID lookups
- **Fleet-wide analytics:** GSI enables "all events of type X" queries
- **Event timeline:** Pure append-only log, no sentinel noise to filter

### What becomes harder
- **Migration:** One-time data migration required (old tables → new schema)
- **Backward compatibility:** All Lambda code must be updated simultaneously
- **DST edge case:** ~2 events/year may sort by wall-clock rather than chronological order

### Scale considerations
At current scale (1–1,000 devices, ~96K events/day), all decisions are appropriate. At larger scale (10K+ devices), revisit:

- **MT timestamps → UTC** if sub-second accuracy matters during November fall-back
- **FilterExpression → composite GSI key** if per-device event volume exceeds ~1,000/day
- **Single events table → partition-level hot-key mitigation** if individual device write rates spike
- **Fleet-wide GSI queries → pagination and caching** if `event_type` partitions grow large

These are optimization decisions, not architectural changes — the table structure supports all of them via GSI additions.

## Alternatives Considered

1. **Separate tables per event type** — Rejected: loses interleaved timeline for debugging.
2. **UTC timestamps** — Rejected: less readable in console; Mountain Time is deployment timezone.
3. **Composite sort key (`type#timestamp`)** — Rejected: less readable, over-engineered.
4. **Keep sentinels in events table** — Rejected: pollutes event log, O(n × sentinels) for fleet overview.
5. **ISO 8601 with timezone offset** — Rejected: longer strings, no benefit for single-timezone deployment.
