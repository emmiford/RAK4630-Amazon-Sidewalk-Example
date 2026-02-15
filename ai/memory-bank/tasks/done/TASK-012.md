# TASK-012: Validate WattTime MOER threshold for PSCO region

**Status**: MERGED DONE (2026-02-11, Eero)
**Branch**: `feature/testing-pyramid`

## Summary
Analysis script created and executed. WattTime free tier only provides current signal index (not historical data). Current MOER consistently at 70% for PSCO region. Recommendation: keep 70% threshold. Full historical analysis requires WattTime paid tier.

## Deliverables
- `aws/scripts/moer_analysis.py`
- `ai/memory-bank/tasks/moer-threshold-analysis.md`
