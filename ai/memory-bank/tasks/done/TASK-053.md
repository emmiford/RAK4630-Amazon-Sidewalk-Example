# TASK-053: Resolve two app_tx.c naming collision

**Status**: merged done (2026-02-15, Eliel)
**Priority**: P1
**Size**: S (1 point)

Renamed platform's `src/app_tx.c` → `src/tx_state.c` with `tx_state_*` functions and new `include/tx_state.h`. Updated all platform callers (`app.c`, `platform_api_impl.c`, `sid_shell.c`, `CMakeLists.txt`). App-side `app_tx.c`/`app_tx.h` unchanged. All tests pass (55/55 + 13/13).
