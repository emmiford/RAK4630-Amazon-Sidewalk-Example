# TASK-105: Naming consistency audit and cleanup

**Status**: merged done (2026-02-21, Utz)
**Priority**: P2
**Owner**: Utz
**Branch**: `task/105-naming-consistency`
**Size**: M (5 points)

## Description
Comprehensive naming consistency audit across firmware (C), cloud (Python/Lambda), infrastructure (Terraform), and documentation. Three rounds of changes:
1. Fix function prefixes, include guards, dead code, SideCharge→device epoch renames, env var casing
2. Remove overloaded EVSE_ prefix from names that don't refer to the charger hardware
3. Rename PIN_BUTTON → PIN_CHARGE_NOW_BUTTON for clarity

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] All function names follow module prefix convention (e.g., `evse_j1772_state_to_string`, `thermostat_inputs_flags_get`)
- [x] Include guards follow `FILENAME_H` pattern (no `__` prefix, no stale names)
- [x] Dead code removed (unused `sidewalk_transfer_t`, `sidewalk_payload_t`, stale declarations)
- [x] All `sidecharge`/`SideCharge` references removed from source code
- [x] EVSE_MAGIC → TELEMETRY_MAGIC, EVSE_PAYLOAD_SIZE → TELEMETRY_PAYLOAD_SIZE
- [x] EVSE_PIN_* → PIN_* (PIN_CHARGE_BLOCK, PIN_COOL, PIN_CHARGE_NOW_BUTTON)
- [x] evse-decoder → uplink-decoder (Terraform Lambda + IAM)
- [x] evse-device-registry → device-registry, evse-daily-aggregates → daily-aggregates
- [x] evse-alerts → monitor-alerts
- [x] 15/15 C tests pass
- [x] 333/333 Python tests pass

## Testing Requirements
- [x] All C unit tests pass (15/15)
- [x] All Python tests pass (333/333)

## Deliverables
- 57 files changed across 3 commits on `task/105-naming-consistency`
- Firmware: header guards, function prefixes, dead code removal, pin renames, magic byte renames
- Python: env var casing, epoch rename, magic byte imports from protocol_constants
- Terraform: Lambda, IAM, DynamoDB, SNS resource renames
- Docs: PRD, TDD, data-retention, device-registry-architecture updated
