# TASK-110: Restore uplink deduplication in decode Lambda

**Status**: in progress (2026-02-22, Eliel + Utz)
**Priority**: P1
**Owner**: Eliel (code), Utz (docs)
**Branch**: `task/110-uplink-dedupe`
**Size**: M (3 points)

## Description
Sidewalk delivers the same LoRa uplink multiple times — once per gateway within range (typically 3–5 neighbor Echo devices). The old dedupe table (`sidewalk-v1-device_events_dedupe_v2`) was deleted during the ADR-006 table migration. ADR-007 then made it worse by using `device_timestamp + cloud_ms_fraction` as the sort key, where `cloud_ms_fraction` differs per gateway delivery, guaranteeing each copy gets a unique SK.

**Fix**: Make the SK deterministic by deriving the millisecond fraction from a SHA-256 hash of the raw payload instead of from cloud time. Add a DynamoDB conditional write to skip duplicates and short-circuit all side effects.

## Dependencies
**Blocked by**: none
**Blocks**: dashboard data quality, daily aggregate accuracy

## Acceptance Criteria
- [ ] ADR-008 documents Sidewalk multi-gateway delivery and dedupe strategy
- [ ] TDD §8.1 updated with dedupe flow
- [ ] `compute_event_timestamp_ms()` uses deterministic payload-hash ms fraction
- [ ] `lambda_handler()` uses conditional write for device-timestamped events
- [ ] Duplicate deliveries return early, skipping side effects (TIME_SYNC, scheduler divergence, charge_now, transitions, registry updates)
- [ ] Non-EVSE payloads (OTA, diagnostics) deduplicated via Sidewalk `seq` number
- [ ] Unit tests cover: deterministic ms fraction, duplicate detection, side-effect skipping
- [ ] All existing tests pass

## Testing Requirements
- [ ] New unit tests for dedupe logic
- [ ] Existing `test_decode_evse.py` tests pass unchanged
- [ ] `python3 -m pytest aws/tests/ -v` green

## Deliverables
- `docs/adr/008-uplink-deduplication.md`
- Updated `docs/technical-design.md` §8.1
- Updated `aws/decode_evse_lambda.py`
- Updated `aws/tests/test_decode_evse.py`
