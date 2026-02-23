# TASK-113: Investigate Board #2 BLE registration failure (PSA reboot loop)

**Status**: merged done (2026-02-23, Eliel)
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/113-psa-test`
**Size**: M (3 points)

## Summary
Board #2 (RAK4630 module) CC310 crypto accelerator is defective. `psa_generate_key()` for ECC P-256 hangs indefinitely even with BLE completely off (antenna removed). Root cause confirmed via custom `sid psa test` shell command that exercises PSA key IDs 3, 4, 5 without BLE — Phase 1 (destroy) succeeds, Phase 2 (generate) hangs every time.

**Workaround found:** Cross-chip PSA ITS key transfer. PSA ITS keys (0xF6000, 8KB) are NOT HUK-encrypted — can be copied between chips via pyOCD. Registered cert5 on Board #1 (working CC310), saved PSA ITS, flashed to Board #2. Both boards now on separate certs and working over LoRa.

**Final board state:**
- Board #1: original cert (restored after cert5 registration), LoRa working
- Board #2: cert5 (registered via Board #1, keys transferred), LoRa working

**Board #2 limitations:** Cannot BLE-register independently. If chip-erased, restore from `cert5_psa_its.bin` + `mfg5.hex`.

## Deliverables
- `sid psa test` shell command added to `platform_shell.c` (on branch `task/113-psa-test`)
- Root cause: CC310 hardware defect (ECC P-256 key generation hangs)
- Cross-chip PSA ITS key transfer procedure validated
- Backup files in `sidewalk-projects/`: `board1_psa_its.bin`, `board1_mfg.bin`, `cert5_psa_its.bin`
