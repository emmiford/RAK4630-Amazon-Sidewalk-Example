# EXP-NNN: Title

**Status**: Proposed | In Progress | Concluded
**Verdict**: — | GO | REVERTED | DECLINED | DOCUMENTED
**Type**: Parameter tuning | Feature A/B | Architecture change | Field measurement | Hardware diagnostic | Data analysis | Tooling | Process evaluation | Incident report
**Date**: YYYY-MM-DD → YYYY-MM-DD
**Owner**: Oliver
**Related**: TASK-NNN, ADR-NNN, EXP-NNN

---

## Problem Statement

What motivated this experiment? What's the pain point or opportunity?

## Hypothesis

If [we do X], then [Y will happen], because [Z reasoning].

**Success Metrics**:
- Primary: measurable outcome that determines GO/REVERT
- Guardrail: constraint that must not be violated

## Method

**Variants**:
- Control: current behavior / baseline
- Variant: proposed change

**Procedure**: step-by-step execution plan (commands, configurations, measurements).

## Results

**Decision**: GO | REVERTED | DECLINED | DOCUMENTED
**Commits**: `abc1234` (implementation), `def5678` (revert if applicable)

### Data

Tables, measurements, logs — the evidence.

### Analysis

What the data shows. Engineering interpretation.

## Key Insights

- Lessons learned that apply beyond this experiment
- Surprises, gotchas, things to remember

## References

- Files: `path/to/relevant/code`
- Tasks: TASK-NNN
- ADRs: ADR-NNN
- Other experiments: EXP-NNN, REC-NNN
