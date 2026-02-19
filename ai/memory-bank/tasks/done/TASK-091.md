# TASK-091: Documentation sync — PRD, TDD, commissioning card, project plan

**Status**: merged done (2026-02-19, Pam + Eliel + Utz)
**Priority**: P2
**Branch**: `task/091-gap-analysis-review` (merged + deleted)
**Size**: M (3 points)

## Summary
Three-agent gap analysis comparing all project documentation against the codebase, followed by direct fixes to 7 files. Pam updated 27 stale PRD status labels, Eliel updated the TDD (wire format v0x09, GPIO polarity, selftest, new cmd_auth and health digest sections), and Utz fixed the commissioning card G-terminal safety mislabel plus project plan, privacy policy, known issues, and CLAUDE.md shell commands.

## Deliverables
- `docs/PRD.md` — 18+ status labels NOT STARTED → IMPLEMENTED, heartbeat 60s→15min, TX rate 100ms→5s
- `docs/technical-design.md` — v0x09 wire format, GPIO charge_block→charge_en, selftest 5→4 checks, new §4.5 cmd_auth, new §8.5 health digest, ADR-002/004 in appendix
- `docs/design/commissioning-card-spec.md` — G terminal "fan" → "earth ground" (safety-critical, 3 locations)
- `docs/privacy-policy.md` — "every 5 minutes" → 15-min heartbeat + state-change
- `docs/project-plan.md` — ~35 tasks NOT STARTED → IMPLEMENTED
- `docs/known-issues.md` — KI-003 marked resolved
- `CLAUDE.md` — shell commands 8 → 22, organized platform/app sections
