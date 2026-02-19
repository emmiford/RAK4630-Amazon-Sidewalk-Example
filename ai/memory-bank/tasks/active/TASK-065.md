# TASK-065: AC-priority software interlock + charge_block rename

**Status**: reopened (2026-02-19, Malcolm)
**Priority**: P1
**Owner**: Eliel
**Size**: M (5 points)

## Summary
Added baseline AC-priority logic to firmware. Renamed `charge_enable` → `charge_block` with polarity inversion (HIGH = blocking, LOW = not blocking). Boot sequence reads cool_call before setting GPIO. Runtime: cool_call HIGH → pause charging, cool_call LOW → resume (if no cloud pause). Updated selftest toggle-and-verify for new name/polarity. PRD and TDD doc corrections.

## Session Work (2026-02-19)

### ADC pin swap (completed, on main)
Swapped ADC channel physical pins: pilot voltage AIN0→AIN1, current clamp AIN1→AIN0. Overlay-only change (channel 0 still = pilot in code). Updated docs: TDD §3.1 + §9.1, PRD (9 references), lexicon. All 15 C + 326 Python tests pass. Built and flashed to device.

### Incomplete Rename (reopened)
The `charge_enable` → `charge_block` rename did not fully land on `main`. The following locations still use the old name:

### Firmware
- `boards/rak4631_nrf52840.overlay` — devicetree nodelabel still `charge_enable`, label still "EVSE Charge Enable"
- `src/platform_api_impl.c` — `DT_NODELABEL(charge_enable)` reference
- `src/app_evse/charge_control.c` — constant named `EVSE_PIN_CHARGE_EN`

### Documentation
- `CLAUDE.md` — shell command comments still say "charge_block GPIO" but overlay doesn't match
- `docs/technical-design.md` §9.1 — table row says `charge_enable`

### What "done" looks like
- All firmware references use `charge_block` (nodelabel, variable names, constants)
- Overlay polarity: `GPIO_ACTIVE_HIGH` where HIGH = blocking
- Docs and code are consistent

## Original Deliverables (partially completed)
- Modified `rak4631_nrf52840.overlay` — `charge_enable` → `charge_block` (**NOT on main**)
- Modified `platform_api_impl.c` — GPIO init as `GPIO_OUTPUT_INACTIVE` (**name still old**)
- Modified `charge_control.c` — AC-priority logic, boot-time cool_call read (done)
- Modified `selftest.c` / `selftest.h` — renamed to `charge_block_ok` (done)
- Updated unit tests (done)
- Updated PRD §2.0 status labels, §2.4.1 implementation table (done)
- Updated TDD §6.3 charge control documentation (done)

## Follow-up
- TASK-048b: Charge Now 30-min latch (overrides AC priority) — DONE
- TASK-066: Button re-test clears FAULT_SELFTEST on all-pass — DONE
- TASK-068: charge_block rename propagation across remaining docs (Pam) — DONE (docs only)
