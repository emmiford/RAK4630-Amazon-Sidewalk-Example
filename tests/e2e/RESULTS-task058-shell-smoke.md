# Shell Smoke Test E2E Results — TASK-058

**Date**: 2026-02-17
**Firmware**: main @ e37e1e8 (platform + app rebuilt)
**Tester**: Eero (automated)
**Branch**: task/047-058-device-verification

## Context

TASK-056 split `app.c` into three files: `app.c`, `platform_shell.c`, `sidewalk_dispatch.c`.
This test verifies all shell commands still work on-device after the refactor.

## Results

| Command | Result | Output Summary |
|---------|--------|----------------|
| `sid status` | PASS | Init: STARTED_OK (err=0), Ready: YES, Link: LoRa (0x4), App: LOADED |
| `sid mfg` | PASS | Version: 8, Device ID: bf:c1:de:6b:b9 |
| `sid ota status` | PASS | OTA Status: IDLE |
| `app evse status` | PASS | J1772=E (Error), pilot=211mV, current=2309mA, charge=NO, sim=NO |
| `sid selftest` | PASS | ADC pilot PASS, ADC current PASS, GPIO cool PASS, Charge enable PASS |
| `sid lora` | PASS | Link switched to LoRa (0x4), no crash, Sidewalk re-initialized |
| `sid ble` | PASS | BT initialized (nRF52x, Identity: D0:52:E5:E8:6D:60), no crash |

## Self-Test Detail

Hardware checks all pass. Cross-checks fail as expected without EVSE hardware:
- Clamp match: WARN (floating ADC reads ~2300mA, no load attached)
- Interlock: WARN (current while paused — same floating ADC cause)
- Fault flags: 0x60 = FAULT_CLAMP (0x20) + FAULT_INTERLOCK (0x40)

## Notes

- Boot sequence normal: registered, time sync success (initial), link status down, then READY at ~9s
- Auto-uplink at 60s: `EVSE TX v08: state=4, pilot=263mV, current=2372mA, flags=0x60, ts=0`
- PSA Error -149 on downlink decrypt (HUK invalidated by platform reflash without MFG — KI-002)
- GPIO heat check absent from selftest — removed in v0x08 payload (heat bit dropped)

## Verdict

**PASS** — All 7 shell commands respond correctly after the app.c → platform_shell.c + sidewalk_dispatch.c refactor.
