# TASK-044: PRD update — add commissioning sections, update wiring to G = earth ground

**Status**: not started
**Priority**: P1
**Owner**: Pam
**Branch**: `feature/prd-v1.5`
**Size**: M (3 points)

## Description
The PRD is missing detailed commissioning test sequence (C-01 through C-12), self-test and fault detection section, LED commissioning behavior, and installation failure modes. Additionally, the wiring terminal definitions need to reflect a key design decision: the G terminal is repurposed from HVAC fan control to earth ground (from AC compressor junction box ground screw). The fan wire is permanently dropped.

Key content to add/update:
1. Section 2.5.2: Commissioning Test Sequence (C-01 through C-12)
2. Section 2.5.3: Self-Test and Fault Detection (boot, continuous, on-demand)
3. Section 2.5.4: Installation Failure Modes
4. Section 2.5.1: LED behavior matrix (prototype + production)
5. Wiring terminal definitions: R (24VAC), Y (cool call), C (common), G (earth ground — NOT fan), P (J1772 pilot), CT (current clamp)
6. EVSE connector selection guidance
7. Remove all fan references

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [ ] PRD sections 2.5.1, 2.5.2, 2.5.3, 2.5.4 added with full content
- [ ] All 12 commissioning steps (C-01 through C-12) documented with pass criteria
- [ ] G terminal defined as earth ground (from compressor junction box), not fan
- [ ] C-03 references earth ground from compressor, not thermostat
- [ ] EVSE connector selection guidance included
- [ ] Zero references to fan wire or G = fan anywhere in PRD
- [ ] Known gaps table updated for commissioning card (TASK-041) and self-test (TASK-039)

## Deliverables
- Updated `docs/PRD.md`
