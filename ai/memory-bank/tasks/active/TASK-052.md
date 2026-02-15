# TASK-052: Rename rak_sidewalk → evse_payload

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
`rak_sidewalk.c` aggregates sensor readings into a payload struct. It has nothing to do with the Sidewalk protocol. Rename to match its actual responsibility.

Rename:
- `src/app_evse/rak_sidewalk.c` → `src/app_evse/evse_payload.c`
- `include/rak_sidewalk.h` → `include/evse_payload.h` (if still exists after TASK-051)
- Update all `#include` references (~4 files: app_entry.c, app_tx.c, possibly selftest.c, mock)
- Update `app_evse/CMakeLists.txt` and `tests/Makefile` source lists

Reference: `docs/technical-design-rak-firmware.md`, Change 3.

## Dependencies
**Blocked by**: TASK-051
**Blocks**: none

## Acceptance Criteria
- [ ] No file named `rak_sidewalk.*` remains in the project
- [ ] All references updated (grep for `rak_sidewalk` returns zero hits)
- [ ] App build succeeds
- [ ] Host tests pass

## Testing Requirements
- [ ] App build succeeds
- [ ] 57 host-side tests pass (function names may need updating if prefixed `rak_sidewalk_*`)

## Deliverables
- 2 files renamed, includes updated across ~4-6 files
- Build files updated
