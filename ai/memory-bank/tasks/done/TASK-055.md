# TASK-055: Split ota_update.c into ota_flash.c + ota_update.c

**Status**: merged done (2026-02-18, Eliel)
**Priority**: P2
**Size**: M (3 points)

## Summary
Extracted flash I/O functions (init, erase, write with alignment padding, read, CRC32) into `src/ota_flash.c` + `include/ota_flash.h`. OTA state machine in `ota_update.c` now calls into `ota_flash.h` with no direct flash driver access. All 38 OTA host tests pass, platform build succeeds (589KB), on-device verified (`sid ota status` → IDLE, Sidewalk LoRa connected).
