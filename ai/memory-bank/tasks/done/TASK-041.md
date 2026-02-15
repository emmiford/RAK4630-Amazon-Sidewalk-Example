# TASK-041: Commissioning checklist card — printed card with 12-step checklist and wiring diagram

**Status**: merged done (2026-02-14, Bobby)
**Priority**: P0
**Owner**: Bobby
**Branch**: committed via `task/040-prod-selftest-trigger`, merged to main
**Size**: M (3 points)

## Summary
Double-sided 8.5"x11" quarter-fold commissioning checklist card. Side A has all 12 steps (C-01 through C-12) with pass/fail checkboxes, LED quick reference table, troubleshooting guide, installation record with sign-off fields, and front cover. Side B has full wiring reference diagram with main connection diagram, SideCharge terminal map, current clamp CORRECT/WRONG examples, EVSE connector selection, and NEC caution bar. C-06 includes amber "SELF-TEST REQUIRED" badge with 5-press button instructions (integrating TASK-039/040). C-11 has amber "CRITICAL SAFETY TEST" badge. All text as proper SVG `<text>` elements (rebuilt from vectorized originals).

## Deliverables
- `docs/commissioning-card-source/side-a-checklist.svg` (37KB, portrait 850x1100)
- `docs/commissioning-card-source/side-b-checklist.svg` (19KB, landscape 1100x850)
- `docs/commissioning-card-source/side-a-checklist.pdf` (403KB, generated via Chrome headless)
- `docs/design/commissioning-card-spec.md` (master design spec, 565 lines)
