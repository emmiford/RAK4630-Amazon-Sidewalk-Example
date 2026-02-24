# TASK-072: On-device Charge Now button GPIO verification

**Status**: MERGED DONE (2026-02-17, Eero)
**Priority**: P2
**Owner**: Eero
**Branch**: `task/072-button-gpio-verify` (merged to main, branch removed)
**Size**: XS (1 point)

## Summary
Added `Button GPIO: %d` readout to selftest shell output (`selftest.c`). Verified GPIO reads 0 on device when no button wired (correct pull-down behavior). Software verification complete; physical button hardware test deferred to TASK-103 (validate external button and potentiometer hardware independently).
