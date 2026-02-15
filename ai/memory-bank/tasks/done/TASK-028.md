# TASK-028: Add MFG key health check tests

**Status**: MERGED DONE (2026-02-11, Eero)
**Branch**: `feature/testing-pyramid`

## Summary
7 tests. Extracted `mfg_key_health_check()` from static function in `sidewalk_events.c` into standalone module (`mfg_health.h`/`mfg_health.c`). Returns `mfg_health_result_t` struct with `ed25519_ok` and `p256r1_ok` booleans.

## Deliverables
- `tests/app/test_mfg_health.c` (7 tests)
- `src/mfg_health.c` + `include/mfg_health.h`
- `tests/mocks/sid_pal_mfg_store_ifc.h`
