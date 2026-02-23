# TASK-113: Investigate Board #2 BLE registration failure (PSA reboot loop)

**Status**: not started
**Priority**: P2
**Owner**: —
**Branch**: —
**Size**: M (3 points)

## Description
Board #2 (RAK4631 module) cannot complete Sidewalk BLE registration with ANY certificate. Every attempt follows the same pattern: BLE connects to Echo gateway → `psa_destroy_key invalid id 3/4/5` warnings → board reboots → kicked out of screen. Tested with 3 unique certificates (cert0/Board #1's cert, cert2, and cert5). cert5 was confirmed PROVISIONED (never registered) in AWS IoT Wireless. Same firmware works perfectly on Board #1.

**Key findings from this session:**
- Board #2 requires NanoDAP for power — won't boot from USB alone (RAK19007 baseboard). This suggests the baseboard's USB-to-module power path is broken (dead DAPLink chip may be in the power path).
- NanoDAP provides power via VCC + requires SWDIO + SWCLK connected (3 of 4 pins). GND is shared via USB.
- SWD lines being active during BLE registration may interfere with RF or draw current at wrong moment.
- Board #2 exhibits intermittent SWD brownouts during flash (red LED dims/flashes, "No ACK" errors). Workaround: `pyocd erase --chip -Oconnect_mode=under-reset --frequency 500000`.
- The `sid mfg` Device ID (`bf:c6:9f:4a:aa`) is hardware-derived (HUK), same regardless of cert — this is normal.

**Possible root causes:**
1. NanoDAP can't supply enough current for BLE TX bursts (~10mA peak on top of normal draw)
2. SWD lines interfere with BLE radio during registration
3. This module's PSA key storage or HUK has a hardware defect
4. RAK19007 baseboard USB power path broken (DAPLink in power path)

## Dependencies
**Blocked by**: none
**Blocks**: none (Board #1 is the primary dev board)

## Acceptance Criteria
- [ ] Determine why Board #2 can't complete BLE registration
- [ ] Try: disconnect SWD data lines during BLE registration (leave only VCC/GND from NanoDAP)
- [ ] Try: power module via its own USB port or external 3.3V supply (bypass NanoDAP entirely)
- [ ] Try: power via RAK19007 USB with NanoDAP completely disconnected
- [ ] Try: move module to a different baseboard (RAK19011)
- [ ] If hardware defect confirmed: document and mark module as limited-use (no Sidewalk, GPIO/ADC only)

## Deliverables
- Root cause identified and documented
- Board #2 either registered or documented as defective
