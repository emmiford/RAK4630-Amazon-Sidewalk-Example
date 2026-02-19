# TASK-098: Cloud v1.1 — Decode HEAT flag in Lambda + DynamoDB

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
Update the decode Lambda and DynamoDB schema to handle the HEAT flag (bit 0 of the flags byte) that firmware v1.1 will start reporting.

### Work items
1. **decode_evse_lambda.py**: Extract bit 0 from flags byte as `heat_call` (boolean). Currently only bit 1 (`cool_call`) is decoded.
2. **DynamoDB**: Add `heat_call` field to the telemetry record. Same pattern as `cool_call`.
3. **charge_scheduler_lambda.py**: If heat call is active, treat it the same as cool call for scheduling decisions (compressor has priority, don't send charge-allow downlinks).
4. **Daily aggregation Lambda** (TASK-078): Include `heat_call` in aggregated stats if applicable.

### Backward compatibility
The HEAT flag has been 0 in all prior payloads (bit was reserved). Existing records are unaffected. The Lambda should handle both old payloads (bit 0 = 0) and new payloads (bit 0 = 0 or 1) identically.

## Dependencies
**Blocked by**: TASK-097 (firmware must send the flag first)
**Blocks**: none

## Acceptance Criteria
- [ ] `decode_evse_lambda.py` extracts `heat_call` from flags byte bit 0
- [ ] DynamoDB telemetry record includes `heat_call` field
- [ ] Scheduler treats heat_call same as cool_call for interlock decisions
- [ ] Existing payloads (bit 0 = 0) decoded correctly (backward compatible)
- [ ] Terraform `apply` succeeds with updated Lambda

## Testing Requirements
- [ ] Python unit test: decode payload with HEAT flag set
- [ ] Python unit test: decode payload with HEAT flag clear (backward compat)
- [ ] Python unit test: scheduler respects heat_call for interlock

## Deliverables
- Updated `aws/decode_evse_lambda.py`
- Updated `aws/charge_scheduler_lambda.py`
- Updated `aws/tests/` (new test cases)
- Terraform apply (if schema changes needed)
