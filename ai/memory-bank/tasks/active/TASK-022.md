# TASK-022: BUG — Stale flash data inflates OTA delta baselines

**Status**: coded (device-verified — partition dump clean after erase + flash)
**Priority**: P1
**Owner**: Eero
**Branch**: `task/048-selftest-verify-stale-flash`
**Size**: M (3 points)

## Description
When physically flashing a smaller app over a larger one, pyOCD only erases pages it writes to. Pages beyond the new image retain old code. `ota_deploy.py baseline` captures the full partition trimming only trailing 0xFF — stale non-0xFF bytes survive and inflate the baseline. Same problem after OTA apply: the apply loop only processes pages for the new image size, leaving stale pages from a previous larger image.

**Symptom**: Baseline shows 4524 bytes when actual app is 239 bytes. Delta OTA computes against inflated baseline.

## Dependencies
**Blocked by**: none
**Blocks**: none (but affects OTA delta reliability)

## Acceptance Criteria
- [x] `flash.sh app` erases 0x90000-0xCEFFF before writing app hex
- [x] OTA apply (full, delta, recovery) erases pages beyond new image up to metadata boundary
- [x] `ota_deploy.py baseline` warns if dump is significantly larger than app.bin
- [x] Host-side tests cover stale page erase after apply (5 Python tests)
- [x] Manual verification: flash app → dump partition → all bytes beyond app are 0xFF (8444B used, 249604B erased)

## Implementation (three-layer defense)
1. **flash.sh**: `pyocd erase --sector 0x90000+0x3F000` before `pyocd flash`
2. **ota_update.c**: `erase_stale_app_pages()` called after apply loop in `ota_apply()`, `ota_resume_apply()`, `delta_apply()`
3. **ota_deploy.py**: `pyocd_dump()` warns if trimmed dump is > 2× app.bin size

## Device Verification (2026-02-17)
- Partition dump: 8444 bytes used, 249604 bytes erased (100% clean)
- pyocd notes: `cmd -c "erase ..."` fails with flash init timeout — must use `pyocd erase --sector`
- pyocd notes: `--no-erase` flag doesn't exist — removed from flash.sh

## Deliverables
- Updated `flash.sh` (PYOCD variable, erase --sector, removed --no-erase)
- Updated `src/ota_update.c` (erase_stale_app_pages)
- Updated `aws/ota_deploy.py` (baseline size warning)
- 5 new Python tests in `aws/tests/test_ota_deploy_cli.py`
