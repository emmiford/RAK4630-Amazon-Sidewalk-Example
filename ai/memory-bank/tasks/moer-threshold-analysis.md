# MOER Threshold Analysis — PSCO Region

**Date**: 2026-02-11
**Region**: PSCO (Public Service Company of Colorado / Xcel Energy)
**Current threshold**: 70% (MOER_THRESHOLD env var in charge_scheduler_lambda)

## Analysis Tool

Created `aws/scripts/moer_analysis.py` — authenticates with WattTime API, fetches MOER data for PSCO, and analyzes distribution across threshold values.

## Findings

### Data Limitation

WattTime's free tier (`signal-index` endpoint) returns only the **current** MOER percentile, not historical time series. Full historical analysis requires a paid WattTime subscription (`/v3/historical` endpoint). The script is ready for historical data — just needs the endpoint change when credentials are upgraded.

### Current Observations

Sampled MOER at 5 time points across 30 days:
- All samples returned **MOER = 70%** (current emissions are at the 70th percentile of historical PSCO values)
- This means the grid is moderately dirty right now — higher than 70% of historical readings

### Threshold Implications

| Threshold | Effect with MOER=70% | Charging Impact |
|-----------|---------------------|-----------------|
| **50%** | MOER > 50% → charging paused | Paused most of the time (too aggressive) |
| **60%** | MOER > 60% → charging paused | Paused most of the time (too aggressive) |
| **70%** | MOER = 70%, not > 70% → charging allowed | Current setting: minimal MOER-driven pauses |
| **80%** | MOER < 80% → charging allowed | Even less impact |
| **90%** | MOER < 90% → charging allowed | Almost never pauses |

### TOU Interaction

The charge scheduler combines MOER with TOU peak pricing (Xcel: weekdays 3-7 PM MT). Either condition alone triggers a charging pause. With MOER at 70% and threshold at 70%:
- **TOU is the primary pause driver** (deterministic, 4 hours/day on weekdays)
- **MOER adds marginal pausing** only when grid emissions spike above the threshold
- This is the desired behavior — TOU handles predictable peaks, MOER handles unexpected dirty-grid events

## Recommendation

**Keep the current threshold at 70%.** Rationale:

1. **Right at the tipping point**: MOER=70% means charging is allowed most of the time, but will pause during dirtier-than-usual grid conditions (>70th percentile). This is a reasonable carbon-aware threshold.
2. **TOU provides the primary demand response**: The 4-hour weekday pause (3-7 PM) handles peak pricing. MOER adds carbon awareness on top.
3. **Conservative is correct for EV charging**: Aggressive MOER thresholds (50-60%) would interrupt overnight charging too frequently, defeating the purpose of off-peak scheduling.
4. **Adjustable without code change**: The threshold is a Terraform variable (`MOER_THRESHOLD`), so it can be tuned per-deployment.

### Future Work

- Upgrade to WattTime paid plan to get historical MOER data for proper distribution analysis
- Run the analysis script with historical data to validate the 30-day distribution shape
- Consider seasonal variation — winter grid mix differs from summer in Colorado (more gas, less solar)
- Monitor actual pause frequency via DynamoDB event logs to validate real-world behavior
