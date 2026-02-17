# TASK-062: Wire up Charge Now button GPIO end-to-end

**Status**: not started
**Priority**: P2
**Owner**: Eliel
**Branch**: —
**Size**: S (2 points)

## Description
The Charge Now button is defined in the app layer (`EVSE_PIN_BUTTON 3` in
`selftest_trigger.h`) but has no backing hardware path — no devicetree node,
no `gpio_dt_spec`, no case in `platform_gpio_get()`. Today any
`api->gpio_get(3)` call returns `-EINVAL`. Wire it up fully so the selftest
trigger (5-press detection) and future single-press behavior actually read
the physical pin.

## Dependencies
**Blocked by**: none
**Blocks**: TASK-049b (Platform button callback — GPIO interrupt)

## Acceptance Criteria
- [ ] Devicetree overlay (`boards/rak4631_nrf52840.overlay`) has a `charge_now_button` node under a `gpio-keys` compatible, with the correct nRF52840 pin, pull-up/pull-down, and active level
- [ ] `platform_api_impl.c`: `GPIO_PIN_3` defined, `gpio_dt_spec` declared, `platform_gpio_init()` configures it as input, `platform_gpio_get()` switch handles `GPIO_PIN_3`
- [ ] `docs/technical-design.md` §9.1 pin mapping table updated with GPIO 3 row (abstract pin, nRF52840 pin, function, direction, usage)
- [ ] `selftest_trigger.c` 5-press detection works on real hardware (manual verify: 5 presses → LED blink code)
- [ ] Host-side unit tests still pass (`make -C tests/ clean test`) — mock_platform already returns 0 for any pin index, so no mock changes expected

## Physical Pin Selection
**Decide before implementation**: Which nRF52840 GPIO pin is the button wired to on the board? Check the physical wiring / RAK5005-O baseboard schematic. Likely candidates on the WisBlock IO slot:
- P0.07 (WB_IO2) — unused in overlay
- P0.25 (WB_IO1) — unused in overlay

If not yet wired, pick a pin and document the choice.

## Testing Requirements
- [ ] Host-side C tests pass (no regressions)
- [ ] On-device: `app selftest` via shell confirms button GPIO reads 0 when not pressed, 1 when pressed
- [ ] On-device: 5-press selftest trigger fires and blink codes display

## Deliverables
- `boards/rak4631_nrf52840.overlay` — new `charge_now_button` node
- `src/platform_api_impl.c` — GPIO_PIN_3 plumbing
- `docs/technical-design.md` §9.1 — new row in pin mapping table
