# TASK-114: ADR-010 — Amazon Sidewalk module selection (RAK4630)

**Status**: committed (2026-02-25, Eliel)
**Priority**: P3
**Owner**: Eliel
**Branch**: `task/114-adr-module-selection`
**Size**: XS (1 point)

## Description
Document the hardware module selection decision as ADR-010. RAK4630 (nRF52840 + SX1262) chosen over TI/SiLabs/ST dev boards, Seeed LoRa-E5 Mini, and cheaper RAK clones. Key factors: FCC pre-approval, sub-quarter size, BLE for Sidewalk SDK compatibility, Nordic/Zephyr driver alignment.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] ADR-010 written with Context, Decision, Alternatives Considered, Consequences, Risks
- [x] ADR index (README.md) updated with ADR-010 row
- [ ] Merged to main

## Deliverables
- `docs/adr/010-sidewalk-module-selection.md`
- `docs/adr/README.md` (updated index)
