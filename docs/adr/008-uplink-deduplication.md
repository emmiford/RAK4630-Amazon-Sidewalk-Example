# ADR-008: Sidewalk Uplink Deduplication

## Status
Accepted

## Context

Amazon Sidewalk uses LoRa, a broadcast radio protocol. When a device transmits an uplink, every Sidewalk gateway (Amazon Echo) within range receives it independently. Each gateway forwards its copy to AWS IoT Core, which delivers each copy to the IoT rule as a separate Lambda invocation. In a residential setting, 3-5 neighbor Echo devices typically receive each uplink.

The original system had a dedicated DynamoDB dedupe table (`sidewalk-v1-device_events_dedupe_v2`) that tracked sequence numbers to filter duplicates. ADR-006 deleted this table as an orphan during the table migration.

ADR-007 then introduced device timestamps as the DynamoDB sort key, using `device_timestamp_unix * 1000 + cloud_ms_fraction` for sub-second uniqueness. The `cloud_ms_fraction` is derived from the cloud receive time (`cloud_timestamp_ms % 1000`), which differs slightly for each gateway delivery due to gateway-to-cloud latency variation. This guarantees each duplicate gets a unique sort key, defeating any implicit deduplication.

The result: every uplink produces 3-5 DynamoDB rows with identical sensor data but different timestamps and gateway metadata. This inflates storage, corrupts dashboard time series, and skews daily aggregates.

## Decision

**Deduplicate by making the DynamoDB sort key deterministic for the same payload.**

### EVSE telemetry (device-timestamped)

Replace the variable `cloud_ms_fraction` with a deterministic millisecond fraction derived from the SHA-256 hash of the raw payload content:

```python
ms_fraction = int(hashlib.sha256(raw_payload_b64.encode()).hexdigest()[:4], 16) % 1000
```

Same uplink payload -> same hash -> same ms fraction -> same sort key. The first Lambda invocation writes the item; subsequent invocations attempt the same PK+SK and are caught by a DynamoDB conditional write:

```python
table.put_item(
    Item=item,
    ConditionExpression="attribute_not_exists(device_id) AND attribute_not_exists(timestamp_mt)"
)
```

On `ConditionalCheckFailedException`, the Lambda logs the duplicate and returns early, skipping all side effects: TIME_SYNC, scheduler divergence check, charge_now override, transition event storage, device-state update, and registry update.

### OTA, diagnostics, and pre-sync EVSE (cloud-timestamped)

These payload types use cloud receive time as the sort key and are less frequent. They are deduplicated using the same conditional write -- the first write wins. Since cloud-timestamped events have naturally unique SKs (different cloud times per gateway), each gateway delivery writes a separate row. At the low frequency of these events (a few per day), this is acceptable.

### Collision risk

The SHA-256-based ms fraction maps to 0-999. Two different payloads at the same device-second with the same ms fraction would collide (probability ~0.1%). At the current event rate (4 events/hour), this means one potential collision per ~104 days. If it happens, the conditional write blocks the second event -- a minor data loss acceptable for a monitoring system. The `cloud_received_mt` attribute preserves the arrival time for debugging.

## Consequences

### What becomes easier
- **Correct dashboard data**: Each device-timestamped uplink produces exactly one DynamoDB row
- **Accurate aggregates**: Daily stats reflect true event counts, not gateway fanout
- **Reduced costs**: 3-5x fewer DynamoDB writes and side-effect invocations for EVSE telemetry
- **Simpler debugging**: No need to mentally group duplicate rows

### What becomes harder
- **Deterministic SK changes test fixtures**: Existing tests that relied on cloud_ms_fraction for SK computation need updating to pass `raw_payload_b64`
- **Hash computation cost**: SHA-256 per invocation -- negligible (~1us) vs. Lambda runtime

## Supplements

- **ADR-006**: Table migration that deleted the original dedupe table
- **ADR-007**: Device timestamp as sort key -- this ADR fixes the unintended dedupe regression
