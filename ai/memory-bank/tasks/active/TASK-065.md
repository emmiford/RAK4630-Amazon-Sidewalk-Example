# TASK-065: TDD §6.5 self-test rewrite + PRD fault lifecycle

**Status**: in progress (2026-02-17, Pam)
**Priority**: P1
**Owner**: Pam
**Branch**: `task/065-selftest-tdd-prd`
**Size**: M (5 points)

## Description

Documentation quality pass: propagate the `charge_block` rename across all design
documents, update stale implementation status fields, and ensure the TDD §6.5
self-test section and PRD fault lifecycle are consistent and design-correct.

The firmware rename (`charge_enable` → `charge_block` with inverted polarity) was
merged in the AC-priority interlock work, but the documentation was not fully
updated. This task fixes every stale reference across TDD, PRD, project plan,
lexicon, commissioning card spec, and product decisions.

### What changed

**Rename semantics:**
- Old: `charge_enable` — HIGH = allow charging, LOW = pause
- New: `charge_block` — HIGH = block charging, LOW = not blocking (hardware safety gate controls)
- On MCU power loss, GPIO floats LOW (not blocking) — hardware safety gate stays in control

**Documents updated:**
1. **TDD** — §6.3 (charge control pin + polarity), §6.5.1 (boot self-test check 5),
   §6.5.1 rationale, §9.1 (pin mapping table)
2. **PRD** — lexicon (§glossary), §2.0.1 (boot default status → IMPLEMENTED), §2.4.1
   (boot sequence + implementation table statuses → IMPLEMENTED), §2.5.3 (boot
   self-test table, continuous monitoring table, safety check paragraph, status
   table, failure modes, P0 items), §2.6 (shell command descriptions), §7.4
   (hardware requirements: relay description, fail-safe default, self-test
   support), EXP-001 references, task cross-reference table
3. **Project plan** — Feature 1.2.1 tasks (statuses updated), Milestone 2.4
   heading + story, Epic 2 description, Phase 1 reference
4. **Project plan HTML** — same changes mirrored
5. **Lexicon** — Cp pin definition
6. **Commissioning card spec** — C-11 warning bar, troubleshooting section 4
7. **Commissioning card SVG** — warning text
8. **Product decisions** — PDL-006 boot default decision (updated to charge_block + IMPLEMENTED)

### Status field updates
- PRD §2.0.1: "Default state on boot" → IMPLEMENTED (SW+HW) (TASK-065)
- PRD §2.4.1 implementation table: all 4 items → IMPLEMENTED (with task references)
- Product decisions PDL-006: DECIDED → DECIDED — IMPLEMENTED (TASK-065)
- Project plan Feature 1.2.1: 4 tasks marked [x] IMPLEMENTED

## Dependencies

**Blocked by**: none
**Blocks**: TASK-048b (Charge Now 30-min latch), TASK-066 (button re-test clears FAULT_SELFTEST)

## Acceptance Criteria

- [x] Zero `charge_enable` / `charge_en` / `CHARGE_EN` references in docs/
- [x] TDD §6.3 uses `EVSE_PIN_CHARGE_BLOCK` with correct polarity description
- [x] TDD §6.5.1 check 5 says "GPIO charge_block writable"
- [x] TDD §9.1 pin table says `charge_block` (HIGH=block, LOW=not blocking)
- [x] PRD lexicon updated for charge_block terminology
- [x] PRD §2.0.1 boot default status → IMPLEMENTED
- [x] PRD §2.4.1 boot sequence uses charge_block with correct polarity
- [x] PRD §2.4.1 implementation table all marked IMPLEMENTED
- [x] PRD §2.5.3 all references updated (boot table, continuous table, safety paragraph, status table, failure modes)
- [x] PRD §2.6 shell commands describe charge_block polarity
- [x] PRD §7.4 hardware requirements use charge_block
- [x] Project plan Feature 1.2.1 tasks marked IMPLEMENTED
- [x] Project plan Milestone 2.4 renamed to "Charge Block Output"
- [x] Commissioning card spec + SVG updated
- [x] Product decisions PDL-006 updated
- [ ] Commit and merge to main

## Deliverables

- Modified `docs/technical-design.md` — §6.3, §6.5.1, §9.1
- Modified `docs/PRD.md` — lexicon, §2.0.1, §2.4.1, §2.5.3, §2.6, §7.4, appendix references
- Modified `docs/project-plan.md` — Feature 1.2.1, Milestone 2.4, Epic 2, Phase 1
- Modified `docs/project-plan.html` — same changes mirrored
- Modified `docs/lexicon.md` — Cp pin definition
- Modified `docs/design/commissioning-card-spec.md` — C-11, troubleshooting
- Modified `docs/commissioning-card-source/side-a-checklist.svg` — warning text
- Modified `ai/memory-bank/decisions/product-decisions.md` — PDL-006
