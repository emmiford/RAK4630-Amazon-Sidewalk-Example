# TASK-036: Device registry — DynamoDB table with SC- short ID

**Status**: MERGED DONE (2026-02-14, Eliel)
**Branch**: `feature/device-registry` (merged, branch deleted)

## Summary
`aws/device_registry.py` (84 lines), `aws/terraform/device_registry.tf` (41 lines), 12 Python tests. Decode Lambda integration (best-effort last_seen update on every uplink). Auto-provisions on first uplink with status "active". SC-XXXXXXXX short IDs from SHA-256 of Sidewalk UUID. Two GSIs (owner_email, status). Note: Terraform NOT applied yet — see TASK-049.

## Deliverables
- `aws/device_registry.py`
- `aws/terraform/device_registry.tf`
- `aws/tests/test_device_registry.py` (12 tests)
- Updated `aws/decode_evse_lambda.py`
