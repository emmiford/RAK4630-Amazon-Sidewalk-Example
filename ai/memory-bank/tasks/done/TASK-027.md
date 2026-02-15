# TASK-027: Add shell command dispatch tests

**Status**: MERGED DONE (2026-02-11, Eero)
**Branch**: `feature/testing-pyramid`

## Summary
31 tests covering entire `app_on_shell_cmd()` dispatch using capture callback pattern for shell output verification. Tests evse status/a/b/c/allow/pause, hvac status, sid send, unknown subcommand, and NULL safety.

## Deliverables
- `tests/app/test_shell_commands.c` (31 tests)
