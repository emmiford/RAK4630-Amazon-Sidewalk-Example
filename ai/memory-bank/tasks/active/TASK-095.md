# TASK-095: PCB design update — 3 relays, 11 terminals, inline interlock

**Status**: not started
**Priority**: P2
**Owner**: —
**Branch**: —
**Size**: L (5 points)

## Description
Update the PCB design to reflect the inline pass-through wiring architecture. The circuit board must support 11 field-wirable terminals, 3 relays, and the hardware mutual exclusion interlock.

### Terminal layout (11 total)
**Thermostat side (7):**
| Terminal | Signal | Direction |
|----------|--------|-----------|
| R | 24VAC hot | In (power) |
| C | 24VAC common | In (power return) |
| Y-in | Cool call from thermostat | In |
| Y-out | Cool call to compressor | Out (relay-controlled) |
| W-in | Heat call from thermostat | In |
| W-out | Heat call to compressor | Out (relay-controlled) |
| G | Earth ground from compressor junction box | In |

**EVSE side (4):**
| Terminal | Signal | Direction |
|----------|--------|-----------|
| PILOT-in | J1772 Cp from EVSE | In |
| PILOT-out | J1772 Cp to vehicle (or spoofed) | Out (relay-controlled) |
| CT+ | Current clamp + | In (analog) |
| CT- | Current clamp - | In (analog) |

### Relay design (3 relays)
1. **Charge block relay** (GPIO P0.06): Controls PILOT-out. Energized = pass-through; de-energized = ~900Ω spoof (State B).
2. **Y-out relay** (pin TBD): Controls cool call pass-through. Energized = pass-through; de-energized = blocked.
3. **W-out relay** (pin TBD): Controls heat call pass-through. Energized = pass-through; de-energized = blocked.

### Hardware interlock
The circuit must prevent Y-out + charge AND W-out + charge from being active simultaneously, independent of the MCU. If the MCU loses power, all three relays de-energize: charging paused (safe), compressor blocked (safe).

## Dependencies
**Blocked by**: TASK-094 (merged docs define the spec)
**Blocks**: TASK-096

## Acceptance Criteria
- [ ] PCB schematic includes 11 terminals with correct signal routing
- [ ] 3 relays placed with appropriate isolation/creepage
- [ ] Hardware interlock circuit prevents simultaneous compressor + charger operation
- [ ] Fail-safe: MCU power loss → all relays de-energize → both loads off
- [ ] W-out relay GPIO pin assigned and documented
- [ ] PCB layout accommodates existing analog inputs (AIN0, AIN1) and digital inputs (P0.04, P0.05)

## Testing Requirements
- [ ] Schematic review against PRD v1.6 §7.4.1
- [ ] Interlock truth table verified (no state allows compressor + charger simultaneously)
- [ ] Relay coil current within MCU GPIO drive capability (or add driver transistors)

## Deliverables
- Updated PCB schematic
- Updated BOM (3 relays + drivers)
- Assigned GPIO pin for W-out relay
- Interlock truth table
