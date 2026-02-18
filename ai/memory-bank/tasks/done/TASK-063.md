# TASK-063: Delay window support — device storage + scheduler format change

**Status**: merged done (2026-02-17, Eliel)
**Priority**: P1
**Owner**: Eliel
**Branch**: task/063-delay-window
**Size**: L (8 points)

## Summary
Replaced fire-and-forget pause/allow charge control with time-bounded delay
windows `[start, end]` in SideCharge epoch. Device pauses autonomously during
the window and resumes on expiry — no cloud "allow" needed. Handles lost LoRa
downlinks via heartbeat re-send (>30 min stale). Legacy commands still work
(subtype 0x00/0x01) and clear active windows.

## Deliverables
- `delay_window.c` / `delay_window.h` — device-side window storage + check
- `charge_control.c` — integrated window check into poll cycle tick
- `app_rx.c` — routes subtype 0x02 to delay_window, legacy to charge_control
- `app_entry.c` — init delay_window module
- `charge_scheduler_lambda.py` — sends delay windows for TOU/MOER, heartbeat re-send
- 18 new C unit tests (115 total), 12 new Python tests (204 total)
- TDD updated: §4.1 (wire format), §6.3 (charge control), §8.2 (scheduler)
