# TASK-100: Remap firmware pin assignments to RAK19007 + RAK4631 WisBlock connector

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: task/100-wisblock-pin-remap
**Size**: M (5 points)

## Description
The current devicetree overlay uses raw nRF52840 pins (P0.02–P0.07) that are **not routed through the WisBlock connector**. The RAK19007 base board J11 header only exposes 3 usable pins: A1 (P0.31/AIN7), IO1 (P0.17), IO2 (P1.02). The overlay, platform API, and all documentation must be updated to use these physical pins.

### Current (broken for WisBlock)
| Signal | nRF52840 | WisBlock | Accessible? |
|--------|----------|----------|-------------|
| Pilot voltage (ADC) | P0.03 / AIN1 | — | NO |
| Current clamp (ADC) | P0.02 / AIN0 | — | NO |
| Charge block (output) | P0.06 | — | NO |
| Cool call (input) | P0.05 | — | NO |
| Heat call (input) | P0.04 | IO4/A0 | Not on J11 |
| Charge Now button | P0.07 | — | NO |

### Target (RAK19007 J11 header)

| RAK19007 J11 | WisBlock Label | RAK4631 Pin | nRF52840 | Analog | Signal |
|---|---|---|---|---|---|
| Pin 1 | A1 | WB_A1 | P0.31 | AIN7 | **Pilot voltage** (ADC input) |
| Pin 2 | IO1 | WB_IO1 | P0.17 | — | **Charge block** (GPIO output) |
| Pin 3 | IO2 | WB_IO2 | P1.02 | — | **Cool call** (GPIO input) |
| Pin 4 | VBAT | — | — | — | Battery voltage (not used) |

### Dropped signals (no pins available)
- **Current clamp** — no second analog pin; report 0 mA, rely on pilot state
- **Heat call** — future heat pump, not needed for v1.0
- **Charge Now button** — use cloud command or shell instead

### IO2 / 3V3_S concern
On the RAK19007, IO2 (P1.02) also controls the 3V3_S switched power rail. Using it as cool_call input requires confirming no driver conflict with the base board's 3V3_S enable circuit. If conflict exists, swap charge_block and cool_call pin assignments.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-095 (PCB design), TASK-096 (W-out relay GPIO)

## Acceptance Criteria
- [ ] Overlay updated: ADC channel 0 → NRF_SAADC_AIN7 (P0.31) for pilot
- [ ] Overlay updated: charge_block GPIO → P0.17 (IO1)
- [ ] Overlay updated: cool_call GPIO → P1.02 (IO2)
- [ ] Overlay updated: remove heat_call, charge_now, current clamp ADC channel
- [ ] platform_api_impl.c compiles with new pin assignments
- [ ] Selftest updated — only 2 checks remain (ADC pilot + charge_block toggle)
- [ ] evse_sensors.c gracefully handles missing current clamp (return 0)
- [ ] charge_now.c disabled or stubbed (no button GPIO)
- [ ] TDD §9.1 pin mapping table updated
- [ ] PRD §2.0.3 pin table updated
- [ ] All C unit tests pass
- [ ] All Python tests pass
- [ ] Platform + app build succeeds
- [ ] Verified on physical RAK19007: pilot ADC reads voltage on J11 pin 1

## Testing Requirements
- [ ] Host-side C tests pass (15 executables)
- [ ] Python Lambda tests pass (326 tests)
- [ ] On-device: `app evse status` shows pilot voltage via J11 A1 pin
- [ ] On-device: `app evse allow` / `app evse pause` toggles J11 IO1
- [ ] On-device: cool_call input readable via J11 IO2

## Deliverables
- Modified `boards/rak4631_nrf52840.overlay`
- Modified `src/platform_api_impl.c`
- Modified `src/app_evse/evse_sensors.c` (current clamp stub)
- Modified `src/app_evse/selftest.c` (reduced check count)
- Modified `src/app_evse/charge_now.c` (disabled/stubbed)
- Modified `src/app_evse/thermostat_inputs.c` (remove heat_call)
- Updated `docs/technical-design.md` §9.1
- Updated `docs/PRD.md` §2.0.3
- Updated `CLAUDE.md` pin references
