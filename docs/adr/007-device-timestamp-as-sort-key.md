# ADR-007: Device Timestamp as DynamoDB Sort Key

## Status
Accepted

## Context

ADR-006 established Mountain Time strings as the DynamoDB sort key format. However, the *source* of the timestamp was always cloud receive time — when AWS received the LoRa uplink.

The device embeds a device-side timestamp (seconds since 2026-01-01, via TIME_SYNC) in every v0x07+ telemetry payload. For real-time uplinks this is approximately the same as cloud time. But for **buffered events** — queued in the device's ring buffer while offline and drained when connectivity resumes — the cloud receive time can be hours after the actual event.

This causes two problems:
1. **Misordered events:** A State B→C transition that happened at 2 PM gets a sort key of 6 PM (when the buffer drained), appearing *after* real-time events from 3–6 PM.
2. **Difficult queries:** To find "when did J1772 transition to state C?" requires scanning the nested `data.evse.device_timestamp_unix` attribute instead of using the sort key range.

## Decision

**Use device timestamp as the sort key when available; fall back to cloud time otherwise.**

A new `compute_event_timestamp_ms()` function determines the effective timestamp:

| Payload Type | Condition | SK Source | `timestamp_source` |
|---|---|---|---|
| EVSE telemetry | `device_timestamp_epoch > 0` | Device time + cloud ms fraction | `"device"` |
| EVSE telemetry | `device_timestamp_epoch == 0` or missing | Cloud receive time | `"cloud_presync"` |
| OTA, diagnostics | Always | Cloud receive time | `"cloud"` |
| Scheduler commands | Always | Cloud receive time | `"cloud"` |

Sub-second uniqueness: device timestamps have second resolution (ADR-002). The millisecond fraction from cloud receive time (0–999) is appended to provide uniqueness when multiple events share the same device-second.

Two new attributes are added to every DynamoDB record:
- **`timestamp_source`** (`"device"` | `"cloud"` | `"cloud_presync"`): Indicates what time source populated the sort key.
- **`cloud_received_mt`**: Always stores when AWS received the message (Mountain Time string), regardless of SK source.

The `last_seen` field in the device-state table uses `cloud_received_mt` (not the effective SK), because "last seen" is a connectivity metric — when we last heard from the device.

TTL remains cloud-time-based: 90 days from receipt, not from event time.

## Consequences

### What becomes easier
- **Correct event ordering:** Buffered events sort by when they happened, not when they arrived
- **Time-range queries:** `timestamp_mt BETWEEN '2026-02-22 14:00' AND '2026-02-22 15:00'` returns events that *occurred* in that window
- **Debugging:** No need to scan nested `data.evse.device_timestamp_unix` to reconstruct true timelines
- **Auditability:** `cloud_received_mt` preserves the original cloud arrival time for latency analysis

### What becomes harder
- **Mixed SK semantics:** Old records (pre-migration) have no `timestamp_source` attribute. Queries spanning old and new data must handle this gracefully (absence = cloud time).
- **Clock drift:** If device time drifts significantly from true time, events will sort at the wrong position. Out of scope per ADR-002 (second-resolution sync is sufficient for current use cases).

### Interlock transitions
`store_transition_event` uses the same effective timestamp (+1ms offset for SK collision avoidance) and includes both `cloud_received_mt` and `timestamp_source`.

## Supplements

- **ADR-002**: Time sync uses second resolution — this ADR inherits that precision for device-sourced SKs.
- **ADR-006**: Table architecture — this ADR changes *what time* populates the MT string SK, not the format.
