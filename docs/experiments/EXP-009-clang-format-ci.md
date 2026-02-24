# EXP-009: clang-format CI Enforcement

**Status**: Concluded
**Verdict**: DECLINED
**Type**: Process evaluation
**Date**: 2026-02-11
**Owner**: Oliver

---

## Problem Statement

Code formatting is enforced only by convention. A CI-enforced formatter would prevent style drift and reduce review friction.

## Hypothesis

Adding clang-format to CI will keep C code consistently formatted without manual effort.

**Success Metrics**:
- Primary: zero formatting violations in CI

## Method

**Options Evaluated**:
1. Add config + enforce in CI — one reformatting commit, then CI keeps it consistent
2. Skip it — code is already consistently styled by hand; cppcheck catches real bugs
3. Add config but warn-only — run in CI as informational, don't fail the build

## Results

**Decision**: DECLINED — Option 2 (Skip it)

### Analysis

- Codebase uses **tabs for indentation** (standard in Zephyr/embedded)
- clang-format defaults expect spaces — every line would trigger a violation without config
- Even with correct config (`UseTab: ForIndentation`, `IndentWidth: 8`, `TabWidth: 8`), clang-format would likely reformat existing code (brace placement, `#define` alignment, etc.)
- Risk: a "format-only" commit would pollute git blame and touch files unnecessarily
- Current state: code is already consistently styled by hand across the codebase
- cppcheck is already in CI and catches real bugs — higher value per CI second

**Rationale**: The cost (reformatting risk, config tuning, git blame pollution) outweighs the benefit for a single-developer embedded project with already-consistent style. cppcheck in CI provides the real value. Revisit if team grows or style drift becomes a problem.

## Key Insights

- For small teams with consistent style, CI format enforcement adds overhead without proportional benefit.
- The git blame pollution cost of a reformatting commit is often underestimated.
- Static analysis (cppcheck) > formatting enforcement for embedded projects.
