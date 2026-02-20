# TASK-065: AC-priority software interlock + charge_block rename

**Status**: done (2026-02-19, Eliel)
**Priority**: P1
**Owner**: Eliel
**Size**: M (5 points)
**Branch**: `task/065-charge-block-rename`

## Summary

Complete rename of `charge_enable` → `charge_block` across all firmware, tests, and documentation. Fixed critical GPIO polarity bug: the rename changed the name but not the signal values. With `charge_block`, HIGH = blocking (spoof engaged, EVSE sees no vehicle) and LOW = not blocking (pilot passes through, EVSE allowed). On MCU power loss, GPIO floats LOW → EVSE operates normally (safe default).

## Changes (19 files)

### Firmware (6 files)
- `rak4631_nrf52840.overlay` — nodelabel `charge_enable` → `charge_block`, label updated
- `platform_api_impl.c` — DT_NODELABEL, variable names, comments → `charge_block`
- `charge_control.c` — `EVSE_PIN_CHARGE_EN` → `EVSE_PIN_CHARGE_BLOCK`, **polarity inversion**: `allowed ? 0 : 1` (was `allowed ? 1 : 0`), all gpio_set calls corrected
- `selftest.c` — `EVSE_PIN_CHARGE_EN` → `EVSE_PIN_CHARGE_BLOCK`, `charge_en_ok` → `charge_block_ok`
- `selftest.h` — `charge_en_ok` → `charge_block_ok`, FAULT_INTERLOCK comment updated
- `selftest_trigger.c` — `charge_en_ok` → `charge_block_ok`
- `app_entry.c` — shell messages now say `charge_block low/high` instead of `GPIO high/low`

### Tests (5 files)
- `test_app.c` — renamed test functions (pause=high, allow=low), flipped GPIO assertions
- `test_charge_control.c` — flipped init/allow/pause GPIO assertions
- `test_shell_commands.c` — flipped allow/pause GPIO assertions
- `test_selftest_trigger.c` — `charge_en` → `charge_block` in comments
- `tests/e2e/RESULTS-selftest.md`, `RESULTS-task058-shell-smoke.md`, `RUNBOOK.md` — "Charge enable" → "Charge block"

### Documentation (3 files)
- `docs/technical-design.md` — §6.3 polarity description rewritten (LOW=pass-through, HIGH=spoof), §6.5 selftest refs, §9.1 pin table, §9.3.2 relay table, §9.3.3 MCU power loss behavior
- `docs/PRD.md` — pin table, fault descriptions, boot default decision (PDL-006)
- `docs/lexicon.md` — "Charge enable GPIO set HIGH" → "Charge block GPIO set LOW"

## Key Design Point

**No power to PCB = EVSE allowed.** The relay is normally-closed (pass-through). The MCU must actively assert `charge_block` HIGH to block charging. This is the safe default — if the SideCharge device loses power or crashes, the EVSE works as if the device isn't there.

## Test Results
- 15/15 C unit tests pass (341 assertions)
- 326/326 Python tests pass
