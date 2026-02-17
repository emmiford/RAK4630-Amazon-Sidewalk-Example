# TASK-065: AC-priority software interlock + charge_block rename

**Status**: merged done (2026-02-17, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: task/065-ac-priority-interlock
**Size**: M (5 points)

## Summary
Added baseline AC-priority logic to firmware. Renamed `charge_enable` → `charge_block` with polarity inversion (HIGH = blocking, LOW = not blocking). Boot sequence reads cool_call before setting GPIO. Runtime: cool_call HIGH → pause charging, cool_call LOW → resume (if no cloud pause). Updated selftest toggle-and-verify for new name/polarity. PRD and TDD doc corrections.

## Deliverables
- Modified `rak4631_nrf52840.overlay` — `charge_enable` → `charge_block`
- Modified `platform_api_impl.c` — GPIO init as `GPIO_OUTPUT_INACTIVE`
- Modified `charge_control.c` — AC-priority logic, boot-time cool_call read
- Modified `selftest.c` / `selftest.h` — renamed to `charge_block_ok`
- Updated unit tests
- Updated PRD §2.0 status labels, §2.4.1 implementation table
- Updated TDD §6.3 charge control documentation

## Follow-up
- TASK-048b: Charge Now 30-min latch (overrides AC priority)
- TASK-066: Button re-test clears FAULT_SELFTEST on all-pass
