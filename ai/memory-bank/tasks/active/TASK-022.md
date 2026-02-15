# TASK-022: BUG — Stale flash data inflates OTA delta baselines

**Status**: not started
**Priority**: P1
**Owner**: —
**Branch**: `fix/stale-flash-erase`
**Size**: M (3 points)

## Description
When physically flashing a smaller app over a larger one, pyOCD only erases pages it writes to. Pages beyond the new image retain old code. `ota_deploy.py baseline` captures the full partition trimming only trailing 0xFF — stale non-0xFF bytes survive and inflate the baseline. Same problem after OTA apply: the apply loop only processes pages for the new image size, leaving stale pages from a previous larger image.

**Symptom**: Baseline shows 4524 bytes when actual app is 239 bytes. Delta OTA computes against inflated baseline.

**Plan**: Saved at `~/.claude/plans/witty-painting-matsumoto.md` — NOT YET APPROVED. Three-layer defense-in-depth:
1. `flash.sh`: Erase app partition before writing (primary fix)
2. `ota_update.c`: Erase stale pages after OTA apply — local flash op, no extra OTA chunks (~5s)
3. `ota_deploy.py`: Warn if baseline is significantly larger than app binary

## Dependencies
**Blocked by**: none
**Blocks**: none (but affects OTA delta reliability)

## Acceptance Criteria
- [ ] `flash.sh app` erases 0x90000-0xCEFFF before writing app hex
- [ ] OTA apply (full, delta, recovery) erases pages beyond new image up to metadata boundary
- [ ] `ota_deploy.py baseline` warns if dump is significantly larger than app.bin
- [ ] Host-side tests cover stale page erase after apply
- [ ] Manual verification: flash large app → flash small app → dump partition → all bytes beyond small app are 0xFF

## Testing Requirements
- [ ] C unit tests for stale page erase logic
- [ ] Python tests for baseline size warning

## Deliverables
- Updated `flash.sh`
- Updated `src/ota_update.c`
- Updated `aws/ota_deploy.py`
- New tests
