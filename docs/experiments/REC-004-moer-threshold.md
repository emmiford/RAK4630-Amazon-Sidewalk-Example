# REC-004: WattTime MOER Threshold Optimization (Currently 70%)

**Status**: Proposed (preliminary analysis complete)
**Verdict**: —
**Type**: Data analysis
**Priority**: High
**Owner**: Oliver
**Related**: `aws/charge_scheduler_lambda.py`, `aws/scripts/moer_analysis.py`

---

## Problem Statement

The charge scheduler pauses charging when MOER exceeds 70%. This threshold was set without analysis of PSCO region MOER distributions. It may be too aggressive (excessive charging interruption) or too lenient (minimal carbon benefit).

## Hypothesis

Analyzing historical MOER data for the PSCO region will reveal an optimal threshold that maximizes carbon displacement while minimizing charging interruption.

**Success Metrics**:
- Primary: hours of charging interrupted per day at each threshold
- Secondary: estimated CO2 displacement (lbs) at each threshold
- Guardrail: vehicle must reach full charge by morning (enough off-peak hours)

## Preliminary Analysis (2026-02-11)

**Region**: PSCO (Public Service Company of Colorado / Xcel Energy)
**Tool**: `aws/scripts/moer_analysis.py` — authenticates with WattTime API, fetches current MOER data.

### Data Limitation

WattTime's free tier (`signal-index` endpoint) returns only the **current** MOER percentile, not historical time series. Full historical analysis requires a paid subscription (`/v3/historical` endpoint). The script is ready for historical data.

### Threshold Impact at Current MOER=70%

| Threshold | Effect with MOER=70% | Charging Impact |
|-----------|---------------------|-----------------|
| **50%** | MOER > 50% → paused | Paused most of the time (too aggressive) |
| **60%** | MOER > 60% → paused | Paused most of the time (too aggressive) |
| **70%** | MOER = 70%, not > 70% → allowed | Current setting: minimal MOER-driven pauses |
| **80%** | MOER < 80% → allowed | Even less impact |
| **90%** | MOER < 90% → allowed | Almost never pauses |

### TOU Interaction

The charge scheduler combines MOER with TOU peak pricing (Xcel: weekdays 3-7 PM MT). Either condition alone triggers a pause. With MOER at 70% and threshold at 70%:
- **TOU is the primary pause driver** (deterministic, 4 hours/day on weekdays)
- **MOER adds marginal pausing** only when grid emissions spike above the threshold

### Preliminary Recommendation

**Keep the current threshold at 70%.** Rationale:
1. Right at the tipping point — charges most of the time, pauses during dirtier-than-usual grid conditions
2. TOU provides the primary demand response (4-hour weekday pause)
3. Conservative is correct for EV charging — aggressive MOER thresholds interrupt overnight charging too frequently
4. Adjustable without code change (`MOER_THRESHOLD` Terraform variable)

## Method (Remaining)

**Step 1**: Upgrade to WattTime paid plan to get historical MOER data
**Step 2**: Pull 30 days of PSCO MOER data, plot distribution, identify natural break points
**Step 3**: Simulate charging behavior at thresholds 50%, 60%, 70%, 80%, 90%
**Step 4**: 1-week A/B per threshold if warranted
**Duration**: Analysis phase (1 day), then 1-week per threshold

### Future Work

- Run analysis script with historical data to validate 30-day distribution shape
- Consider seasonal variation — winter grid mix differs from summer in Colorado (more gas, less solar)
- Monitor actual pause frequency via DynamoDB event logs to validate real-world behavior

## References

- Files: `aws/charge_scheduler_lambda.py`, `aws/scripts/moer_analysis.py`
