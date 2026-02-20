# TASK-100: Pin reassignment — move EVSE GPIOs/ADCs to WisBlock-accessible pins

**Status**: not started
**Priority**: P1
**Owner**: Eliel + Pam + Utz
**Branch**: —
**Size**: M (3 points)

## Description
The current EVSE pin assignments use nRF52840 GPIOs (P0.06, P0.07, P0.02) that are NOT routed to the RAK4631 WisBlock connector. They cannot be tested on the RAK19007 base board. Reassign all EVSE pins to WisBlock-accessible pins, update the overlay, and update all documentation with triple-label pin tables (nRF52840 pin, WisBlock/RAK4631 name, RAK19007 silkscreen label).

### Proposed pin mapping

| Signal | Current Pin | New Pin | WisBlock Name | RAK19007 Label |
|--------|-------------|---------|---------------|----------------|
| charge_block (output) | P0.06 | P0.17 | WB_IO1 | J11 IO1 |
| cool_call (input) | P0.05 | P1.02 | WB_IO2 | J11 IO2 |
| charge_now (input) | P0.07 | P0.09 | WB_IO5 | IO slot |
| heat_call (input) | P0.04 | P0.04 | WB_IO4 | IO slot (unchanged) |
| pilot ADC (analog) | P0.03 / AIN1 | P0.05 / AIN3 | WB_A0 | IO slot |
| current ADC (analog) | P0.02 / AIN0 | P0.31 / AIN7 | WB_A1 | J11 AIN1 |

### Rationale
- The 3 most-needed test signals (charge_block, cool_call, current ADC) land on J11 header pins accessible without disassembly
- All ADC pins remain on SAADC-capable AINx inputs
- heat_call stays on P0.04 (already WisBlock-accessible via IO slot)

## Dependencies
**Blocked by**: TASK-065
**Blocks**: none

## Acceptance Criteria
- [ ] Overlay updated with new pin assignments (`rak4631_nrf52840.overlay`)
- [ ] TDD §9.1 pin table uses triple labels (nRF52840 / WisBlock / RAK19007)
- [ ] All TDD pin references updated (§6.3, §9.3, signal path diagrams)
- [ ] PRD §2.0.3 pin tables updated with triple labels
- [ ] All PRD pin references updated (~15 locations)
- [ ] `docs/lexicon.md` pin references updated
- [ ] `docs/project-plan.md` pin references updated
- [ ] All C unit tests pass (app code uses abstract pin indices — no code changes expected)
- [ ] All Python tests pass
- [ ] Platform + app build succeeds
- [ ] Device flashed and verified (`app evse status` shows correct defaults)

## Testing Requirements
- [ ] C unit tests pass (`cmake ... && ctest`)
- [ ] Python tests pass (`pytest aws/tests/ -v`)
- [ ] Platform build succeeds (west build)
- [ ] App build succeeds (cmake)
- [ ] On-device verification after flash

## Deliverables
- Modified `boards/rak4631_nrf52840.overlay` — new pin assignments
- Modified `docs/technical-design.md` — triple-label pin tables, updated references
- Modified `docs/PRD.md` — triple-label pin tables, updated references
- Modified `docs/lexicon.md` — updated pin references
- Modified `docs/project-plan.md` — updated pin references
