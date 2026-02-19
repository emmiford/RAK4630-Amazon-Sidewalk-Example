# TASK-044: PRD update — add commissioning sections, update wiring to G = earth ground

**Status**: MERGED DONE (2026-02-16, Pam)
**Priority**: P1
**Owner**: Pam
**Branch**: `task/044-prd-commissioning`
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
- [x] PRD sections 2.5.1, 2.5.2, 2.5.3, 2.5.4 added with full content
- [x] All 12 commissioning steps (C-01 through C-12) documented with pass criteria
- [x] G terminal defined as earth ground (from compressor junction box), not fan
- [x] C-03 references earth ground from compressor, not thermostat
- [x] EVSE connector selection guidance included
- [x] Zero references to fan wire or G = fan anywhere in PRD
- [x] Known gaps table updated for commissioning card (TASK-041) and self-test (TASK-039)

## Deliverables
- Updated `docs/PRD.md`
- Updated `docs/lexicon.md` (added G terminal definition)

## Changes Made
1. **New section 2.0.3.1**: Wiring Terminal Definitions — full installer-facing terminal reference table (R, C, Y, G, PILOT, CT+/CT-) with explicit explanation of why G = earth ground and not fan
2. **New section 2.2.1**: EVSE Connector Selection Guidance — hard requirements (J1772, hardwired, current limits, pilot access) and recommendations for specific charger models
3. **C-03 pass criteria updated**: Now references earth ground from compressor junction box ground screw with pointer to 2.0.3.1
4. **Section 2.5.2 status**: TASK-041 commissioning card design marked as done
5. **Section 2.5.3 status**: All TASK-039 self-test items (boot, continuous, shell command, fault flags) marked IMPLEMENTED (SW)
6. **Section 2.5.4**: Thermostat failure mode row updated with G terminal clarification; P0 items updated with TASK-039/041 done status
7. **Known gaps table**: TASK-039 and TASK-041 marked as resolved with strikethrough
8. **Isolation table**: Earth ground reference updated from "EV charger enclosure" to "AC compressor junction box ground screw (G terminal)"
9. **PCB wiring diagram deliverable**: Terminal mapping updated with all 6 terminals and cross-reference to 2.0.3.1
10. **Lexicon**: Added G Terminal (Earth Ground) entry with definition, context, and "do not use" guidance
11. **PRD status line**: Updated to reflect TASK-044 changes
