# TASK-050: Delete platform-side EVSE shell files

**Status**: merged done (2026-02-15, Eliel)
**Priority**: P1
**Size**: S (1 point)

## Summary
Deleted `src/evse_shell.c` (113 lines) and `src/hvac_shell.c` (51 lines) — boundary violations where platform code imported app-layer headers. Both files were orphaned (never in CMakeLists.txt). Shell commands route through `app_entry.c` `on_shell_cmd` callback.

## Deliverables
- 2 dead files deleted, 164 lines removed
- 55/55 host tests pass, platform compilation unaffected
