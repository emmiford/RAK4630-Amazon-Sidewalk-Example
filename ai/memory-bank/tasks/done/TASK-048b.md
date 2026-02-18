# TASK-048b: Charge Now 30-minute latch — device-side override logic

**Status**: MERGED DONE (2026-02-17, Eliel)
**Priority**: P2
**Branch**: `task/048b-charge-now-latch`

## Summary
Implemented the Charge Now 30-minute latch per ADR-003. Single button press
activates a 30-min override: charging forced on, cloud pause commands ignored,
delay window cleared, FLAG_CHARGE_NOW (bit 3) held in uplinks. Cancels on
expiry, unplug (J1772 state A), or long-press (3s). LED feedback: 3 rapid
blinks on press, 0.5Hz slow blink during override.

## Deliverables
- New `charge_now.c` / `charge_now.h` (latch state machine, timer, cancel logic)
- Modified `selftest_trigger.c` (single-press → Charge Now, long-press → cancel, 5-press → selftest)
- Modified `app_rx.c` (cloud 0x10 commands blocked during latch)
- Modified `app_tx.c` (FLAG_CHARGE_NOW from latch state)
- Modified `app_entry.c` (init, tick, shell status integration)
- 20 new unit tests (135/135 total pass)
