# Experiment Log

**Experiment Tracker**: Oliver
**Project**: RAK Sidewalk EVSE Monitor
**Methodology**: Scientific method — hypothesis, controlled method, measured results, documented insights. Each experiment is one file, immutable once concluded (append addenda, don't edit conclusions).

## Format

Each experiment follows the template in [TEMPLATE.md](TEMPLATE.md). Numbering:
- **EXP-NNN**: Executed experiments (concluded or in progress)
- **REC-NNN**: Proposed experiments (awaiting execution)

## Concluded Experiments

| # | Title | Verdict | Type | Date |
|---|-------|---------|------|------|
| [EXP-001](EXP-001-windowed-blast-ota.md) | Windowed Blast OTA Transfer Mode | REVERTED | Feature A/B | pre-2026-02 |
| [EXP-002](EXP-002-ota-chunk-size.md) | OTA Chunk Size Optimization (12B → 15B) | GO | Parameter tuning | pre-2026-02 |
| [EXP-003](EXP-003-ota-stale-threshold.md) | OTA Retry Stale Threshold (300s → 30s) | GO | Parameter tuning | pre-2026-02 |
| [EXP-004](EXP-004-delta-ota.md) | Delta OTA Mode | GO | Feature A/B | pre-2026-02 |
| [EXP-005](EXP-005-on-change-sensing.md) | On-Change Sensing vs Fixed-Interval Polling | GO | Architecture change | pre-2026-02 |
| [EXP-006](EXP-006-raw-payload.md) | Raw 8-Byte Payload vs sid_demo Protocol | GO | Architecture change | pre-2026-02 |
| [EXP-007](EXP-007-split-image-architecture.md) | Split-Image Architecture (Platform + App) | GO | Architecture change | pre-2026-02 |
| [EXP-008](EXP-008-generic-platform.md) | Generic Platform (All Domain Knowledge to App) | GO | Architecture change | pre-2026-02 |
| [EXP-009](EXP-009-clang-format-ci.md) | clang-format CI Enforcement | DECLINED | Process evaluation | 2026-02-11 |
| [EXP-009b](EXP-009b-board2-pot-overcurrent.md) | Board #2 Potentiometer Overcurrent Incident | DOCUMENTED | Incident report | ~2026-01-05 |
| [EXP-010](EXP-010-rak19001-pin-mapping.md) | RAK19001 Baseboard Pin Mapping Validation | DOCUMENTED | Hardware diagnostic | 2026-02-21 |
| [EXP-011](EXP-011-dashboard-data-generation.md) | Dashboard Data Generation (Lambda Replay + Serial) | GO | Tooling | 2026-02-22 |
| [EXP-012](EXP-012-ain1-recovery-connector.md) | Board #1 AIN1 Pin Recovery + Connector Validation | DOCUMENTED | Hardware diagnostic | 2026-02-22 |
| [EXP-013](EXP-013-j1772-telemetry-analysis.md) | J1772 Telemetry Field Analysis (Feb 2026) | DOCUMENTED | Data analysis | 2026-02-21 |

## Proposed Experiments

| # | Title | Priority | Type |
|---|-------|----------|------|
| [REC-001](REC-001-heartbeat-interval.md) | Heartbeat Interval Optimization | Medium | Parameter tuning |
| [REC-002](REC-002-adc-polling-interval.md) | ADC Polling Interval Optimization | Medium | Parameter tuning |
| [REC-003](REC-003-tx-rate-limiter.md) | TX Rate Limiter Tuning | Low | Parameter tuning |
| [REC-004](REC-004-moer-threshold.md) | WattTime MOER Threshold Optimization | High | Data analysis |
| [REC-005](REC-005-ota-field-reliability.md) | OTA Reliability Under Real-World LoRa | High | Field measurement |
| [REC-006](REC-006-charge-control-auto-resume.md) | Charge Control Auto-Resume Timer Validation | Medium | Field measurement |
| [REC-007](REC-007-post-reseat-pin-validation.md) | Post-Reseat RAK19001 Pin Validation | Critical | Hardware diagnostic |
| [REC-008](REC-008-saadc-errata-workaround.md) | SAADC Errata Workaround Retention Decision | High | Hardware diagnostic |
| [REC-009](REC-009-button-pot-assembly.md) | External Button/Potentiometer Assembly Validation | Medium | Hardware diagnostic |
| [REC-010](REC-010-connector-seating-force.md) | Connector Seating Force Measurement | Medium | Process evaluation |

## Experiment Evolution Timeline

~~~
Commit     Date         Experiment                            Verdict
────────────────────────────────────────────────────────────────────────
e35af07    —            EVSE monitor app created              Foundation
550560f    —            EXP-006: Raw payload (69% reduction)  GO
d6faff4    —            EXP-007: Split-image architecture     GO
deb4007    —            EXP-005: On-change sensing            GO
b8e62cd    —            EXP-002: Chunk size 12→15B (20%)      GO
3db368b    —            EXP-003: Stale threshold 300→30s      GO
e3f97e0    —            EXP-001: Windowed blast mode          REVERTED
65e7389    —            EXP-004: Delta OTA (~100x faster)     GO
78924b6    —            EXP-001 reverted (delta made it moot)
7dab212    —            OTA reliability fixes (supporting)    GO
e88d519    —            EXP-008: Generic platform             GO
—          ~2026-01-05  EXP-009b: Board #2 pot overcurrent    DOCUMENTED
—          2026-02-21   EXP-010: RAK19001 pin validation      DOCUMENTED
cd08a27    2026-02-22   EXP-011: Dashboard data generation    GO
—          2026-02-22   EXP-012: AIN1 recovery + connector    DOCUMENTED
—          2026-02-21   EXP-013: J1772 telemetry analysis     DOCUMENTED
~~~

## Cross-References

- **Task Index**: `ai/memory-bank/tasks/INDEX.md`
- **ADR Index**: `docs/adr/README.md`
- **Tasks informed by experiments**:
  - TASK-005 (OTA recovery tests) — validates EXP-004/EXP-007 recovery paths
  - TASK-007 (E2E test plan) — should include REC-005 field conditions
  - TASK-008 (OTA recovery docs) — documents EXP-007/EXP-008 recovery behavior
  - TASK-104 (SAADC errata workaround) — resolved by EXP-012 Phase 1
  - TASK-108 (timestamp field mismatch) — discovered by EXP-011
  - TASK-113 (Board #2 BLE registration) — informed by EXP-012 hardware inventory

---

**Statistical Confidence**: Experiments in this project are engineering feature flags with before/after comparisons, hardware diagnostics with deterministic pass/fail, or data analyses. No controlled statistical trials. Metrics are quoted from commit messages, code artifacts, and direct measurement.
