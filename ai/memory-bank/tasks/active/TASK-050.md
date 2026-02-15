# TASK-050: Delete platform-side EVSE shell files

**Status**: not started
**Priority**: P1
**Owner**: Eliel
**Branch**: —
**Size**: S (1 point)

## Description
Delete `src/evse_shell.c` and `src/hvac_shell.c` from the platform build. These are boundary violations — platform files that import app-layer headers (`evse_sensors.h`, `charge_control.h`, `thermostat_inputs.h`). They are also dead code: the app already handles `app evse *` and `app hvac *` commands through the `on_shell_cmd` callback in `app_entry.c`.

Reference: `docs/technical-design-rak-firmware.md`, Change 1.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-056

## Acceptance Criteria
- [ ] `src/evse_shell.c` deleted
- [ ] `src/hvac_shell.c` deleted
- [ ] Both removed from `CMakeLists.txt` platform source list
- [ ] Platform builds without errors (`nrfutil toolchain-manager launch ...`)
- [ ] `app evse status` still works via app callback path (manual serial test or E2E)
- [ ] `app hvac status` still works via app callback path
- [ ] Host tests pass (`make -C tests/ clean test`)

## Testing Requirements
- [ ] Platform build succeeds
- [ ] Existing 57 host-side tests pass (no app test changes expected)
- [ ] Manual verification: `app evse status` routes through app_entry.c

## Deliverables
- 2 files deleted, 2 lines removed from CMakeLists.txt
