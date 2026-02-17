# ADR-002: Time Sync Uses Second Resolution in a 4-Byte Field

## Status

Accepted (2026-02-16)

## Context

The device timestamps every uplink with a SideCharge epoch value (§7.1). The current implementation derives time from a millisecond uptime counter but truncates to whole seconds before encoding into the uplink payload as a `uint32_le` (bytes 6-9, 4 bytes).

The question arose: is second-level resolution appropriate, or is it overkill for our use cases? And does the 4-byte timestamp cost too much payload on a 19-byte LoRa MTU?

### What the timestamp is used for

1. **TOU/demand-response scheduling** — The charge scheduler (§8.2) runs cloud-side on EventBridge. It uses wall-clock time from its own environment, not the device timestamp. The device timestamp is irrelevant to scheduling decisions.
2. **Telemetry logging** — DynamoDB records track when state transitions occurred. Minute-level accuracy would be more than sufficient for dashboarding.
3. **Event buffer replay** — When the device buffers events during a Sidewalk outage (§6.5, up to 50 entries), the timestamp on each entry lets the cloud reconstruct temporal ordering. This is the primary consumer of timestamp accuracy and the reason the field exists at all.
4. **ACK watermark trimming** — The watermark is monotonically increasing; resolution doesn't affect correctness.

### Actual accuracy

- **Wire resolution**: 1 second (epoch value is whole seconds).
- **Crystal drift**: nRF52840 32.768 kHz crystal drifts ±100 ppm = ±8.6 seconds/day.
- **Re-sync cadence**: Daily via TIME_SYNC downlink, so worst-case error is ~9 seconds.
- **Stated target**: 5-minute accuracy (§7.3) — easily met.

### Payload cost

The timestamp occupies 4 bytes of a 10-byte uplink (v0x08), consuming 21% of the 19-byte LoRa MTU. The previous format v0x06 had no timestamp and was only 8 bytes. However, the remaining 9 bytes of MTU headroom are sufficient for all planned fields.

## Decision

**Keep second resolution in a 4-byte `uint32_le` field.** The resolution is more than needed, but reducing it would not save any wire bytes — a `uint32_t` is already the smallest practical container for a multi-year epoch counter, and packing sub-second bits or switching to a 2-byte minute counter would add complexity for negligible gain:

- A 2-byte minute counter covers only 45 days from epoch before wrapping — insufficient.
- A 3-byte second counter covers ~194 days — marginal, and non-aligned reads complicate both firmware and Lambda decode.
- A 4-byte second counter covers 136 years — simple, aligned, and future-proof.

The 4-byte payload cost is justified by event buffer replay fidelity. Without timestamps, buffered events received in a batch after an outage would all share the same cloud-receive time, losing the actual sequence and timing of state transitions.

## Consequences

**What becomes easier:**
- Event buffer replay faithfully reconstructs when state transitions happened, not just when uplinks were received
- Simple encode/decode — native 32-bit integer, no packing tricks
- No timestamp-related changes needed for years

**What becomes harder:**
- 4 bytes of the 19-byte MTU are permanently allocated to the timestamp, leaving 9 bytes of headroom for future fields
- If MTU pressure increases (new sensor data, longer payloads), the timestamp would be the first candidate for compression or conditional inclusion

## Alternatives Considered

1. **Drop the timestamp entirely (v0x06 style)**: Saves 4 bytes but loses event buffer replay ordering. Rejected — the event buffer exists specifically to survive outages, and timestamps make the buffered data meaningful.

2. **2-byte minute-resolution counter**: Wraps after 45 days. Would require either a shorter epoch base or periodic re-basing. Too fragile for the marginal 2-byte savings.

3. **3-byte second counter**: 194-day range is workable but tight. Unaligned field complicates both C struct packing and Python decode. Saves only 1 byte. Not worth the complexity.

4. **Conditional inclusion (send timestamp only when buffered)**: Adds branching to encode/decode paths and a flag bit. The 4-byte cost is low enough that always-on is simpler and more robust.

## References

- `docs/technical-design.md` §3.1 — Uplink wire format (v0x08)
- `docs/technical-design.md` §7 — Time Sync (epoch, derivation, drift)
- `docs/technical-design.md` §6.5 — Event buffer
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c` — Uplink encoding
- `aws/decode_evse_lambda.py` — Uplink decoding
