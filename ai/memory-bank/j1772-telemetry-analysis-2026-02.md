# J1772 Telemetry Analysis — February 2026

> **Source**: DynamoDB table `sidewalk-v1-device_events_v2`, queried 2026-02-21.
> **Device**: `b319d001-6b08-4d88-b4ca-4d2d98a6d43c` (only device in table).
> **Note**: The DynamoDB table was subsequently destroyed. This document is the only surviving record of this data.

## Summary

Between Feb 4–21 2026, the device reported 57,629 total items (24,374 with EVSE telemetry). Only 2 of 11 days showed stable J1772 readings. The pilot voltage/state was unreliable from Feb 9 onward, with the problem escalating over time.

---

## Daily Overview

All times are **Mountain Standard Time (UTC-7)**.

| Date       | Msgs   | J1772 States Seen         | Voltage Range (mV) | Current Range (mA) | Verdict        |
|------------|-------:|---------------------------|--------------------:|-------------------:|----------------|
| 2026-02-04 |     66 | B                         | 2819–2819 (Δ0)     | 3073–3073 (Δ0)    | **STABLE**     |
| 2026-02-08 |     99 | D                         | 228–260 (Δ32)      | 2254–2563 (Δ309)  | **STABLE**     |
| 2026-02-09 |  3,458 | UNKNOWN                   | 20360–20360 (Δ0)   | 1972–10190         | Stuck garbage  |
| 2026-02-10 |  2,157 | B, D, UNKNOWN             | 0–37213             | 1354–4554          | State changes  |
| 2026-02-11 |    369 | B, C, D, UNKNOWN          | 0–37225             | 463–30795          | WANDERING      |
| 2026-02-12 |     78 | UNKNOWN                   | 20044–20044 (Δ0)   | 75–61515           | Stuck garbage  |
| 2026-02-17 |    136 | D, UNKNOWN                | 199–20044           | 75–49227           | State changes  |
| 2026-02-18 |    428 | D                         | 182–284 (Δ102)     | 2009–2545          | Voltage drift  |
| 2026-02-19 |     28 | C, D, E, UNKNOWN          | 168–1113            | 2063–58667         | WANDERING      |
| 2026-02-20 | 17,108 | A, B, C, D, E, UNKNOWN    | 0–3269              | 0–59956            | **WANDERING**  |
| 2026-02-21 |    447 | A, B, C, D, E, UNKNOWN    | 0–2825              | 0–36100            | **WANDERING**  |

---

## 0 mV (Fully Grounded) Readings

| Date       | Total Msgs | 0 mV Count | % at 0 mV |
|------------|----------:|-----------:|-----------:|
| 2026-02-04 |        66 |          0 |      0.0%  |
| 2026-02-08 |        99 |          0 |      0.0%  |
| 2026-02-09 |     3,458 |          0 |      0.0%  |
| 2026-02-10 |     2,157 |          1 |      0.0%  |
| 2026-02-11 |       369 |          4 |      1.1%  |
| 2026-02-12 |        78 |          0 |      0.0%  |
| 2026-02-17 |       136 |          0 |      0.0%  |
| 2026-02-18 |       428 |          0 |      0.0%  |
| 2026-02-19 |        28 |          0 |      0.0%  |
| 2026-02-20 |    17,108 |        377 |      2.2%  |
| 2026-02-21 |       448 |        114 |     25.4%  |

Escalating trend: essentially zero until Feb 20, then a quarter of Feb 21 readings fully grounded.

---

## State Distribution on Wandering Days

### 2026-02-10 (2,157 messages)
| State   | Count | %     |
|---------|------:|------:|
| B       |    19 |  0.9% |
| D       |    33 |  1.5% |
| UNKNOWN | 2,105 | 97.6% |

### 2026-02-11 (369 messages)
| State   | Count | %     |
|---------|------:|------:|
| B       |     2 |  0.5% |
| C       |     2 |  0.5% |
| D       |   262 | 71.0% |
| UNKNOWN |   103 | 27.9% |

### 2026-02-19 (28 messages)
| State   | Count | %     |
|---------|------:|------:|
| C       |     2 |  7.1% |
| D       |     7 | 25.0% |
| E       |    18 | 64.3% |
| UNKNOWN |     1 |  3.6% |

### 2026-02-20 (17,108 messages)
| State   | Count  | %     |
|---------|-------:|------:|
| A       |    373 |  2.2% |
| B       |      2 |  0.0% |
| C       |      6 |  0.0% |
| D       |     24 |  0.1% |
| E       |  9,195 | 53.7% |
| UNKNOWN |  7,508 | 43.9% |

### 2026-02-21 (447 messages)
| State   | Count | %     |
|---------|------:|------:|
| A       |     2 |  0.4% |
| B       |    17 |  3.8% |
| C       |    49 | 11.0% |
| D       |   109 | 24.4% |
| E       |   217 | 48.5% |
| UNKNOWN |    53 | 11.9% |

---

## Hourly Breakdown: Feb 9–10 (MST)

The Feb 9–10 episode was a stuck-garbage-value ADC failure, not wandering.

```
  Hour MST     Msgs  States                       Avg mV  Range mV
  ────────────────────────────────────────────────────────────────────
  02/08 18:00      5  UNKNOWN:5                     20360  20360-20360
  02/08 22:00      8  UNKNOWN:8                     20360  20360-20360
  02/08 23:00     43  UNKNOWN:43                    20360  20360-20360
  02/09 00:00    166  UNKNOWN:166                   20360  20360-20360
  02/09 01:00     56  UNKNOWN:56                    20360  20360-20360
  02/09 02:00    116  UNKNOWN:116                   20360  20360-20360
  02/09 03:00    268  UNKNOWN:268                   20360  20360-20360
  02/09 04:00    252  UNKNOWN:252                   20360  20360-20360
  02/09 05:00    236  UNKNOWN:236                   20360  20360-20360
  02/09 06:00    235  UNKNOWN:235                   20360  20360-20360
  02/09 07:00    239  UNKNOWN:239                   20360  20360-20360
  02/09 08:00    326  UNKNOWN:326                   20360  20360-20360
  02/09 09:00    269  UNKNOWN:269                   20360  20360-20360
  02/09 10:00    357  UNKNOWN:357                   20360  20360-20360
  02/09 11:00    349  UNKNOWN:349                   20360  20360-20360
  02/09 12:00    127  UNKNOWN:127                   20360  20360-20360
  02/09 14:00     16  UNKNOWN:16                    20360  20360-20360
  02/09 15:00     83  UNKNOWN:83                    20360  20360-20360
  02/09 16:00    307  UNKNOWN:307                   20360  20360-20360
  02/09 17:00    297  UNKNOWN:297                   20360  20360-20360
  02/09 18:00    298  UNKNOWN:298                   20360  20360-20360
  02/09 19:00    366  UNKNOWN:366                   20360  20360-20360
  02/09 20:00    334  UNKNOWN:334                   20360  20360-20360
  02/09 21:00    123  UNKNOWN:123                   20360  20360-20360
  02/09 22:00     75  B:17 UNKNOWN:58               24273  1283-32457
  02/09 23:00     55  B:2 D:33 UNKNOWN:20           11371  224-37213
  02/10 00:00     78  UNKNOWN:78                    29186  4-37213
  02/10 01:00     76  UNKNOWN:76                    31989  31989-31989
  02/10 02:00     73  UNKNOWN:73                    31989  31989-31989
  02/10 03:00     75  UNKNOWN:75                    31989  31989-31989
  02/10 04:00     75  UNKNOWN:75                    31989  31989-31989
  02/10 05:00     72  UNKNOWN:72                    31989  31989-31989
  02/10 06:00     74  UNKNOWN:74                    31989  31989-31989
  02/10 07:00     74  UNKNOWN:74                    31989  31989-31989
  02/10 08:00     12  UNKNOWN:12                    29323  0-31989
```

**Pattern**: 20360 mV stuck for ~28 hours → brief transition at 10 PM Feb 9 → stuck at 31989 mV for ~10 hours.

---

## Hourly Breakdown: Feb 20–21 (MST)

This is the active wandering episode the user observed.

```
  Hour MST     Msgs  States                           Avg mV  Range mV
  ────────────────────────────────────────────────────────────────────────
  02/19 17:00      1  E:1                                 219  219-219
  02/19 19:00      5  E:5                                 233  226-243
  02/19 20:00     17  E:17                                232  225-238
  02/19 21:00     99  C:5 D:7 E:81 UNKNOWN:6              360  0-1536
  02/19 22:00     27  A:1 E:18 UNKNOWN:8                  475  0-1024
  02/19 23:00     29  E:17 UNKNOWN:12                     577  252-1024
  02/20 00:00     35  A:1 E:18 UNKNOWN:16                 605  0-1024
  02/20 01:00    672  A:25 E:428 UNKNOWN:219              498  0-1024
  02/20 02:00   1600  A:53 E:935 UNKNOWN:612              544  0-1024
  02/20 03:00   1750  A:40 E:943 UNKNOWN:767              590  0-1024
  02/20 04:00   1895  A:57 E:982 UNKNOWN:856              599  0-1024
  02/20 05:00   1871  A:33 E:965 UNKNOWN:873              614  0-1024
  02/20 06:00   1940  A:33 E:990 UNKNOWN:917              619  0-1024
  02/20 07:00   2008  A:33 E:1028 UNKNOWN:947             618  0-1024
  02/20 08:00   2030  A:38 E:1038 UNKNOWN:954             616  0-1024
  02/20 09:00   1896  A:39 E:1018 UNKNOWN:839             594  0-1024
  02/20 10:00   1173  A:18 E:695 UNKNOWN:460              556  0-1536
  02/20 11:00     60  A:2 B:2 C:1 D:17 E:16 UNKNOWN:22   756  0-3269
  02/20 20:00     77  B:7 C:16 D:21 E:24 UNKNOWN:9        842  0-1993
  02/20 21:00    142  A:2 B:10 C:15 D:47 E:41 UNKNOWN:27  728  0-2825
  02/20 22:00    142  C:15 D:35 E:75 UNKNOWN:17            402  0-1757
  02/20 23:00     10  D:6 E:4                              386  167-722
  02/21 12:00     17  C:3 E:14                             289  0-1517
  02/21 13:00     23  E:23                                   0  0-0
  02/21 14:00     22  E:22                                   0  0-0
  02/21 15:00     18  E:18                                   0  0-0
```

**Pattern**: Firehose of 1600–2000 msgs/hour from 1 AM–10 AM Feb 20, ~50% E / ~45% UNKNOWN / ~2% A, voltage bouncing 0–1024 mV. 9-hour gap midday. Resumed evening with broader state mix. By Feb 21 afternoon, stuck at 0 mV (State E = fully grounded).

---

## Firmware Code Path Analysis

### How J1772 state is determined (`evse_sensors.c`)

Threshold ladder on ADC voltage:
| Threshold Constant        | Value (mV) | Above → State |
|---------------------------|----------:|---------------|
| `J1772_THRESHOLD_A_B_MV` |     2,600 | A (+12V)      |
| `J1772_THRESHOLD_B_C_MV` |     1,850 | B (+9V)       |
| `J1772_THRESHOLD_C_D_MV` |     1,100 | C (+6V)       |
| `J1772_THRESHOLD_D_E_MV` |       350 | D (+3V)       |
| (else)                    |         — | E (0V)        |

### How UNKNOWN is produced

Two code paths:

1. **ADC read failure** (`evse_sensors.c:73-76`): `platform->adc_read_mv()` returns negative error → state = UNKNOWN, error propagated. In `evse_payload.c:54-56`, voltage is set to **0**.

2. **Init failure** (`evse_payload.c:42-47`): `evse_payload_init()` fails → state = UNKNOWN, all fields zero.

**Key observation**: In the current firmware, UNKNOWN always means the ADC read failed, and the voltage should be 0. The Feb 9 data showing UNKNOWN with voltage 20360 mV is inconsistent — either an older firmware version or an older Lambda decoder without the sanity check (`pilot_voltage > 15000 → reject`).

### Simulation values (`app evse a/b/c` commands)

```c
static const uint16_t state_voltages[] = {
    2980, 2234, 1489, 745, 0, 0  // A, B, C, D, E, F
};
```

None of the DynamoDB data matches simulation values. The Feb 9 stuck value (20360) and Feb 10 stuck value (31989) are not simulation outputs. Feb 4's stable B at 2819 mV is a real ADC reading (near the 2600 A/B threshold).

### Lambda decoder sanity checks (`decode_evse_lambda.py`)

Both decode paths (`raw_v1` at line 316 and `sid_demo_legacy` at line 394) reject:
- `j1772_state > 6`
- `pilot_voltage > 15000`
- `current_ma > 100000`

Values like 20360 mV and 31989 mV should have been rejected by these checks, suggesting the Feb 9–10 data predates these sanity checks.

---

## Conclusions

1. **Feb 4 and Feb 8** were the only truly stable days.
2. **Feb 9–10**: Distinct failure mode — ADC returning fixed garbage values (20360, then 31989 mV). Likely floating/disconnected pilot sense pin. Not "wandering" but "stuck broken."
3. **Feb 11–12**: Intermittent — mostly UNKNOWN/D with occasional wild swings.
4. **Feb 17–18**: Partial recovery — mostly State D with minor voltage drift.
5. **Feb 19–21**: Escalating wandering — cycling through all states, voltage 0–3269 mV, current values completely unreliable. By Feb 21 afternoon, stuck at 0 mV (fully grounded).
6. **The 0 mV trend is worsening**: 0% → 2.2% → 25.4% over Feb 20–21.
7. **Root cause hypothesis**: ADC/wiring issue on pilot sense circuit — possibly loose connection, corrosion, or progressive failure. The data pattern (intermittent garbage → unstable readings → fully grounded) is consistent with a degrading physical connection.
