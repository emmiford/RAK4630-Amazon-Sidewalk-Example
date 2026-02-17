# TASK-068: charge_block rename propagation + stale status updates

**Status**: MERGED DONE (2026-02-17, Pam)
**Priority**: P1
**Owner**: Pam
**Branch**: task/065-selftest-tdd-prd (legacy — created before renumber from TASK-065)
**Size**: S (2 points)

## Summary
Documentation quality pass: propagated `charge_block` rename (HIGH=block, LOW=not
blocking) across all design documents (TDD, PRD, project plan, lexicon, commissioning
card, product decisions). Updated stale implementation status fields — PRD §2.0.1 boot
default → IMPLEMENTED, PRD §2.4.1 implementation table → all IMPLEMENTED, PDL-006 →
IMPLEMENTED. 11 files changed.

**Note**: Originally tracked as TASK-065 (Pam), renumbered to TASK-068 to resolve ID
collision with Eliel's AC-priority interlock (also TASK-065, merged earlier).

## Deliverables
- Modified `docs/technical-design.md` — §6.3, §6.5.1, §9.1
- Modified `docs/PRD.md` — lexicon, §2.0.1, §2.4.1, §2.5.3, §2.6, §7.4, appendix refs
- Modified `docs/project-plan.md` + `.html` — Feature 1.2.1, Milestone 2.4, Epic 2
- Modified `docs/lexicon.md`, `docs/design/commissioning-card-spec.md`, SVG
- Modified `ai/memory-bank/decisions/product-decisions.md` — PDL-006
