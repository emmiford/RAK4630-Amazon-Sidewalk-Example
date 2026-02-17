# TASK-062: Wire up Charge Now button GPIO end-to-end

**Status**: merged done (2026-02-17, Eliel)
**Priority**: P2
**Owner**: Eliel
**Branch**: `task/062-charge-now-button-gpio`
**Size**: S (2 points)

## Summary
Wired up Charge Now button GPIO (P0.07 / WB_IO2) end-to-end: devicetree overlay
node (`charge_now_button`, pull-down, active-high), `GPIO_PIN_3` in
`platform_api_impl.c` (dt_spec, init, gpio_get switch case), and TDD §9.1 pin
mapping table row. 55/55 host tests pass. On-device manual verification deferred
to TASK-072.
