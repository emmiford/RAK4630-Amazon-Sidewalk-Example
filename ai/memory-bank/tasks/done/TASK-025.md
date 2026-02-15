# TASK-025: Add OTA chunk receive and delta bitmap tests

**Status**: MERGED DONE (2026-02-11, Eero)
**Branch**: `feature/testing-pyramid`

## Summary
13 C tests covering chunk writes, phase rejection, duplicate handling, delta bitmap, state transitions, and out-of-bounds rejection. 13 Python tests covering `compute_delta_chunks()` edge cases and `build_ota_chunk()` format. Key finding: mock flash alignment issue — `ota_flash_write()` pads unaligned writes with 0xFF.

## Deliverables
- `tests/app/test_ota_chunks.c` (13 C tests)
- Updated `aws/tests/test_ota_sender.py` (13 Python tests)
