# TASK-106: EVSE Fleet Dashboard + Table Migration

**Status**: merged done
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/106-dashboard-migration`
**Size**: XL (13 points)
**Date completed**: 2026-02-22

## Summary
Comprehensive DynamoDB table restructuring (ADR-006) + fleet monitoring dashboard. Renamed all tables to consistent `evse-*` scheme, normalized PK to SC-ID across all tables, changed event sort key from Unix ms to Mountain Time strings, added event-type GSI, extracted sentinel keys into dedicated `evse-device-state` table. Built dashboard API Lambda (4 routes) + single-file HTML frontend (dark theme, vanilla JS). Updated all 7 Lambda files, 6 test files, 10+ documentation files.

## Deliverables
- `docs/adr/006-table-architecture.md` — architectural decisions
- `aws/terraform/device_state.tf` — new device-state table
- `aws/terraform/dashboard.tf` — API Gateway + Lambda infrastructure
- `aws/protocol_constants.py` — `unix_ms_to_mt()` timestamp helper
- `aws/dashboard_api_lambda.py` — REST API (fleet overview, device detail, daily stats, OTA)
- `aws/dashboard/index.html` — single-file dashboard frontend
- `aws/migrate_tables.py` — migration script (old → new schema)
- `aws/tests/test_dashboard_api.py` — 30 dashboard API tests
- Updated: `decode_evse_lambda.py`, `charge_scheduler_lambda.py`, `ota_sender_lambda.py`, `aggregation_lambda.py`, `health_digest_lambda.py`, `sidewalk_utils.py`, `ota.py`
- Updated tests: `test_decode_evse.py`, `test_charge_scheduler.py`, `test_ota_sender.py`, `test_time_sync.py`, `test_aggregation.py`
- Updated docs: README, technical-design, data-retention, provisioning, PRD, device-registry-architecture, utility-identification-scope, terraform README, E2E runbook

## Stats
35 files changed, +3,427 / -353 lines. 395 Python tests passing. 10 commits on branch.

## Known Issue
Old DynamoDB tables were destroyed by `terraform apply` before migration script ran (variable defaults renamed existing resources instead of creating new ones alongside). Historical data lost. Device re-registers on next uplink.
