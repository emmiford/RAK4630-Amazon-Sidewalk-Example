# EXP-013: J1772 Telemetry Field Analysis (Feb 2026)

**Status**: Concluded
**Verdict**: DOCUMENTED
**Type**: Data analysis
**Date**: 2026-02-04 → 2026-02-21
**Owner**: Oliver
**Related**: EXP-009b (AIN naming confusion), EXP-010, EXP-012

> **Source**: DynamoDB table `sidewalk-v1-device_events_v2`, queried 2026-02-21.
> **Device**: `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`.
> **Note**: The DynamoDB table was subsequently destroyed. This document is the only surviving record of this data.

---

## Problem Statement

Between Feb 4–21 2026, the device reported 57,629 total items (24,374 with EVSE telemetry). The pilot voltage/state was unreliable from Feb 9 onward. Understanding the failure pattern was necessary to diagnose the root cause (hardware vs firmware vs ADC).

## Hypothesis

The J1772 pilot sensing circuit on the deployed device would produce stable, reliable voltage readings corresponding to actual vehicle connection states. Any instability would indicate a hardware or firmware fault that needs diagnosis.

**Success Metrics**:
- Primary: stable J1772 state readings matching known vehicle states
- Guardrail: voltage within valid J1772 ranges (0V–12V mapped to ADC)

## Method

Retrospective analysis of DynamoDB telemetry data covering 11 days of device operation. Cross-referenced with firmware code paths (`evse_sensors.c` threshold ladder, `evse_payload.c` UNKNOWN generation) and Lambda decoder sanity checks.

## Results

### Daily Overview

All times Mountain Standard Time (UTC-7).

| Date | Msgs | J1772 States | Voltage Range (mV) | Current Range (mA) | Verdict |
|------|-----:|-------------|-------------------:|-------------------:|---------|
| 2026-02-04 | 66 | B | 2819–2819 (Δ0) | 3073–3073 (Δ0) | **STABLE** |
| 2026-02-08 | 99 | D | 228–260 (Δ32) | 2254–2563 (Δ309) | **STABLE** |
| 2026-02-09 | 3,458 | UNKNOWN | 20360–20360 (Δ0) | 1972–10190 | Stuck garbage |
| 2026-02-10 | 2,157 | B, D, UNKNOWN | 0–37213 | 1354–4554 | State changes |
| 2026-02-11 | 369 | B, C, D, UNKNOWN | 0–37225 | 463–30795 | WANDERING |
| 2026-02-12 | 78 | UNKNOWN | 20044–20044 (Δ0) | 75–61515 | Stuck garbage |
| 2026-02-17 | 136 | D, UNKNOWN | 199–20044 | 75–49227 | State changes |
| 2026-02-18 | 428 | D | 182–284 (Δ102) | 2009–2545 | Voltage drift |
| 2026-02-19 | 28 | C, D, E, UNKNOWN | 168–1113 | 2063–58667 | WANDERING |
| 2026-02-20 | 17,108 | A, B, C, D, E, UNKNOWN | 0–3269 | 0–59956 | **WANDERING** |
| 2026-02-21 | 447 | A, B, C, D, E, UNKNOWN | 0–2825 | 0–36100 | **WANDERING** |

### 0 mV (Fully Grounded) Readings

| Date | Total Msgs | 0 mV Count | % at 0 mV |
|------|----------:|-----------:|-----------:|
| 2026-02-04 | 66 | 0 | 0.0% |
| 2026-02-08 | 99 | 0 | 0.0% |
| 2026-02-09 | 3,458 | 0 | 0.0% |
| 2026-02-10 | 2,157 | 1 | 0.0% |
| 2026-02-11 | 369 | 4 | 1.1% |
| 2026-02-12 | 78 | 0 | 0.0% |
| 2026-02-17 | 136 | 0 | 0.0% |
| 2026-02-18 | 428 | 0 | 0.0% |
| 2026-02-19 | 28 | 0 | 0.0% |
| 2026-02-20 | 17,108 | 377 | 2.2% |
| 2026-02-21 | 448 | 114 | 25.4% |

Escalating trend: essentially zero until Feb 20, then a quarter of Feb 21 readings fully grounded.

### State Distribution on Wandering Days

**2026-02-20 (17,108 messages)**:
| State | Count | % |
|-------|------:|-----:|
| A | 373 | 2.2% |
| B | 2 | 0.0% |
| C | 6 | 0.0% |
| D | 24 | 0.1% |
| E | 9,195 | 53.7% |
| UNKNOWN | 7,508 | 43.9% |

**2026-02-21 (447 messages)**:
| State | Count | % |
|-------|------:|-----:|
| A | 2 | 0.4% |
| B | 17 | 3.8% |
| C | 49 | 11.0% |
| D | 109 | 24.4% |
| E | 217 | 48.5% |
| UNKNOWN | 53 | 11.9% |

### Hourly Breakdown: Feb 9–10 (MST)

Stuck-garbage-value ADC failure, not wandering:

```
  Hour MST     Msgs  States                       Avg mV  Range mV
  ────────────────────────────────────────────────────────────────────
  02/08 18:00      5  UNKNOWN:5                     20360  20360-20360
  02/09 00:00    166  UNKNOWN:166                   20360  20360-20360
  ...
  02/09 22:00     75  B:17 UNKNOWN:58               24273  1283-32457
  02/09 23:00     55  B:2 D:33 UNKNOWN:20           11371  224-37213
  02/10 01:00     76  UNKNOWN:76                    31989  31989-31989
  ...
  02/10 08:00     12  UNKNOWN:12                    29323  0-31989
```

**Pattern**: 20360 mV stuck for ~28 hours → brief transition → stuck at 31989 mV for ~10 hours.

### Hourly Breakdown: Feb 20–21 (MST)

Active wandering episode:

```
  Hour MST     Msgs  States                           Avg mV  Range mV
  ────────────────────────────────────────────────────────────────────────
  02/20 01:00    672  A:25 E:428 UNKNOWN:219              498  0-1024
  02/20 02:00   1600  A:53 E:935 UNKNOWN:612              544  0-1024
  ...
  02/20 08:00   2030  A:38 E:1038 UNKNOWN:954             616  0-1024
  02/20 11:00     60  A:2 B:2 C:1 D:17 E:16 UNKNOWN:22   756  0-3269
  02/21 13:00     23  E:23                                   0  0-0
  02/21 15:00     18  E:18                                   0  0-0
```

**Pattern**: 1600–2000 msgs/hour firehose, ~50% E / ~45% UNKNOWN, voltage 0–1024 mV. By Feb 21 afternoon, stuck at 0 mV.

### Firmware Code Path Analysis

**J1772 threshold ladder** (`evse_sensors.c`):
| Threshold | Value (mV) | Above → State |
|-----------|----------:|---------------|
| A/B | 2,600 | A (+12V) |
| B/C | 1,850 | B (+9V) |
| C/D | 1,100 | C (+6V) |
| D/E | 350 | D (+3V) |
| else | — | E (0V) |

**UNKNOWN production**: ADC read failure (`adc_read_mv()` returns negative error) → state = UNKNOWN, voltage set to 0.

Values like 20360 mV and 31989 mV should have been rejected by Lambda sanity checks (`pilot_voltage > 15000`), suggesting Feb 9–10 data predates those checks.

## Key Insights

1. **Feb 4 and Feb 8** were the only truly stable days.
2. **Feb 9–10**: Distinct failure mode — ADC returning fixed garbage values (20360, then 31989 mV). Likely floating/disconnected pin. Not "wandering" but "stuck broken."
3. **Feb 11–12**: Intermittent — mostly UNKNOWN/D with occasional wild swings.
4. **Feb 17–18**: Partial recovery — mostly State D with minor voltage drift.
5. **Feb 19–21**: Escalating wandering — cycling through all states, voltage 0–3269 mV. By Feb 21 afternoon, stuck at 0 mV.
6. **The 0 mV trend was worsening**: 0% → 2.2% → 25.4% over Feb 20–21.
7. **Root cause (identified later by EXP-009b addendum)**: The firmware was reading from `NRF_SAADC_AIN0` (P0.02 — not on the WisBlock connector) until Feb 19. All data before Feb 20 was floating-pin noise, not a hardware fault. The wandering on Feb 20–21 was from the newly-connected potentiometer on AIN7, which triggered the SAADC mux latch (resolved in EXP-012).

## References

- Source data: DynamoDB `sidewalk-v1-device_events_v2` (destroyed; this doc is the only record)
- Firmware: `app/rak4631_evse_monitor/src/app_evse/evse_sensors.c`, `evse_payload.c`
- Lambda: `aws/decode_evse_lambda.py`
- Related: EXP-009b (AIN naming confusion), EXP-010, EXP-012 (AIN1 recovery)
