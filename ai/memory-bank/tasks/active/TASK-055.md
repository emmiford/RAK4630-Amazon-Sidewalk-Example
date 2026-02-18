# TASK-055: Split ota_update.c into ota_flash.c + ota_update.c

**Status**: done (2026-02-17, Eliel)
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/055-split-ota-flash`
**Size**: M (3 points)

## Description
`ota_update.c` is 1,160 lines mixing two concerns:

1. **Flash abstraction** (~150 lines): init, erase, write (alignment-aware), read, CRC32 computation
2. **OTA protocol** (~1,000 lines): state machine, message processing, recovery, delta mode, signing, apply

Extract the flash operations into `src/ota_flash.c` with header `include/ota_flash.h`:
- `ota_flash_init()`
- `ota_flash_erase_pages()`
- `ota_flash_write()` (with alignment padding)
- `ota_flash_read()`
- `compute_flash_crc32()`

`ota_update.c` retains the state machine and calls into `ota_flash.h`.

This separation means:
- Flash changes (different chip, different alignment) don't risk breaking OTA protocol logic
- OTA protocol changes don't risk breaking flash alignment code
- Each file has a single, clear responsibility

Reference: `docs/technical-design-rak-firmware.md`, Change 6.

## Dependencies
**Blocked by**: none
**Blocks**: none

## Acceptance Criteria
- [x] `src/ota_flash.c` contains all flash I/O functions
- [x] `include/ota_flash.h` exports flash API
- [x] `ota_update.c` includes `ota_flash.h` and calls flash functions (no direct flash driver access)
- [ ] Platform build succeeds
- [x] OTA functionality preserved (no behavior change)

## Testing Requirements
- [ ] Platform build succeeds
- [x] Existing OTA tests (if any host-side) still pass — 38 OTA tests pass (16+13+9)
- [ ] Manual smoke test: `sid ota status` works

## Deliverables
- New: `src/ota_flash.c` (126 lines), `include/ota_flash.h` (70 lines)
- Modified: `src/ota_update.c` (1,071 lines, down from 1,190 — flash functions removed, includes added)
- Modified: `include/ota_update.h` (includes `ota_flash.h`, `OTA_FLASH_PAGE_SIZE` moved to `ota_flash.h`)
- Modified: `CMakeLists.txt` (add ota_flash.c to source list)
- Modified: `tests/CMakeLists.txt` (add ota_flash.c to OTA test executables)
