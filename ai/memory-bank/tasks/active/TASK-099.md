# TASK-099: Commissioning checklist update for inline pass-through wiring

**Status**: not started
**Priority**: P2
**Owner**: Bobby
**Branch**: —
**Size**: S (2 points)

## Description
Update the commissioning checklist card and documentation to reflect the inline pass-through wiring architecture. The installer must verify both input and output terminals for Y (cool call), W (heat call), and PILOT signals.

### Checklist updates

**C-03 (Thermostat and ground wiring):**
- Old: Verify R, Y, C on correct terminals; G connected to earth ground
- New: Verify R, C, Y-in, Y-out on correct terminals. For heat pump installations: verify W-in and W-out. G connected to earth ground from compressor junction box.

**C-04 (J1772 pilot):**
- Old: Pilot signal wire connected to correct terminal on EVSE side
- New: PILOT-in connected to EVSE charger Cp output. PILOT-out connected to vehicle connector Cp. Verify pass-through continuity.

**C-08 (Cool call detection):**
- Unchanged in substance but clarify: "Set thermostat to call for cooling → verify device sees cool call on Y-in"

**C-08b (NEW — Heat call detection, heat pump installations only):**
- Set thermostat to call for heating → verify device sees heat call on W-in
- Verify W-out is blocked when EV is charging
- Verify W-out passes through when EV is not charging

**C-09 (Interlock verification):**
- Update to verify both Y-out AND W-out are blocked during charging (heat pump installations)

### Commissioning card
Update the physical commissioning card source (`docs/commissioning-card-source/`) with the new terminal names and test steps.

## Dependencies
**Blocked by**: TASK-094 (merged docs define the spec)
**Blocks**: none

## Acceptance Criteria
- [ ] Commissioning checklist in PRD §2.5.2 matches updated terminal names
- [ ] C-03 verifies Y-in, Y-out, W-in, W-out (heat pump), R, C, G
- [ ] C-04 verifies PILOT-in and PILOT-out
- [ ] C-08b added for heat pump heat call verification
- [ ] C-09 updated for dual-relay interlock verification
- [ ] Commissioning card source updated

## Testing Requirements
- [ ] Walkthrough with a sample heat pump installation scenario
- [ ] Verify checklist covers all 11 terminals

## Deliverables
- Updated commissioning checklist (PRD §2.5.2)
- Updated commissioning card source files
- New C-08b test step documented
