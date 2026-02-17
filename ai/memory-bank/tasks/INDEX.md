# SideCharge Task Index

## Project
Embedded IoT EVSE monitor over Amazon Sidewalk (LoRa) with OTA firmware updates.
**Stack**: Zephyr RTOS / nRF Connect SDK, C, Python (Lambda), Terraform
**Hardware**: RAK4631 (nRF52840 + SX1262 LoRa)

## Status Pipeline
not started → planned → in progress → coded → committed → pushed → merged done
Special: deferred, declined

## Agent Registry
| Agent | Role |
|-------|------|
| Malcolm | Senior project manager — task lists, backlog, prioritization |
| Oliver | Experiment tracker — A/B tests, scientific methodology |
| Arnold | Pipeline orchestrator — multi-phase dev workflows |
| Bobby | Brand guardian — identity systems, visual identity, voice |
| Vanessa | Visual storyteller — narratives, data viz, multimedia |
| Whitney | Whimsy injector — micro-interactions, playful UX, Easter eggs |
| Zach | Content creator — content strategy, SEO, editorial |
| Eero | Testing architect — CI/CD, unit/integration tests, quality |
| Eliel | Backend architect — system design, APIs, cloud infra |
| Utz | UX architect — CSS systems, layout, information architecture |
| Pam | Product manager — strategy, prioritization, roadmap |

## Open Tasks

### P0 — Must-have before field install
| ID | Status | Owner | Title | Blocked By |
|----|--------|-------|-------|------------|
| TASK-049 | coded | Eliel | Deploy device registry (awaiting physical verify) | — |

### P1 — Required for v1.0
| ID | Status | Owner | Title | Blocked By |
|----|--------|-------|-------|------------|
| TASK-029 | not started | Eliel | Production observability | — |
| TASK-045 | not started | Eliel | ED25519 verify library integration | — |
| TASK-046 | not started | Eero | Signed OTA E2E verification | TASK-045 |
| TASK-047 | not started | — | On-device verification (TIME_SYNC + buffer + v0x08) | — |
| TASK-058 | not started | Eero | On-device shell verification (post app.c refactor) | — |
| TASK-060 | not started | Eliel | Uplink payload v0x08 — remove pilot voltage, remove heat call | — |
| TASK-063 | not started | Eliel | Delay window support (device + cloud) | — |
| TASK-066 | not started | — | Button re-test clears FAULT_SELFTEST on all-pass | TASK-068 |
| TASK-067 | not started | — | LED blink priority state machine (PRD §2.5.1) | — |

### P2 — Important but not blocking v1.0
| ID | Status | Owner | Title | Blocked By |
|----|--------|-------|-------|------------|
| TASK-026 | not started | Eero | Boot path + app discovery tests | — |
| TASK-030 | not started | Eliel | Fleet command throttling | — |
| TASK-032 | not started | Eliel | Cloud command authentication | — |
| TASK-037 | planned | Pam | Utility identification (PRD scoping done) | TASK-049 |
| TASK-038 | not started | Pam | Data privacy — policy + retention + CCPA | — |
| TASK-042 | not started | Pam | Privacy agent | TASK-038 |
| TASK-048b | not started | Eliel | Charge Now 30-min latch (ADR-003) | TASK-065 |
| TASK-054 | not started | Eliel | Shared platform API pointer (replace 10 setters) | — |
| TASK-055 | not started | Eliel | Split ota_update.c → ota_flash.c + ota_update.c | — |
| TASK-057 | not started | Eero | Route selftest through evse_sensors, not direct ADC | — |
| TASK-061 | not started | Eliel | Event buffer — write on state change, not every poll cycle | — |
| TASK-064 | not started | Eliel | Cloud Charge Now protocol (ADR-003) | TASK-048b |
| TASK-069 | not started | Eliel | Interlock transition event logging | — |
| TASK-070 | not started | — | Production heartbeat interval (60s → 15min) | — |
| TASK-071 | not started | Eliel | Scheduler sentinel divergence detection | — |
| TASK-072 | not started | — | On-device Charge Now button GPIO verification | — |

### P3 — Nice-to-have
| ID | Status | Owner | Title | Blocked By |
|----|--------|-------|-------|------------|
| TASK-049b | not started | Eliel | Platform button callback (GPIO interrupt) | — |

## Completed Tasks (45)
| ID | Title | Status | Date | Agent |
|----|-------|--------|------|-------|
| TASK-001 | Merge feature/generic-platform to main | MERGED DONE | 2026-02-11 | Oliver |
| TASK-002 | Create CLAUDE.md | MERGED DONE | 2026-02-11 | — |
| TASK-003 | Update README.md | MERGED DONE | 2026-02-11 | Eero |
| TASK-004 | Charge scheduler Lambda tests | MERGED DONE | 2026-02-11 | — |
| TASK-005 | OTA recovery path tests | MERGED DONE | 2026-02-11 | Eero |
| TASK-006 | Decode Lambda tests | MERGED DONE | 2026-02-11 | — |
| TASK-007 | E2E test plan | MERGED DONE | 2026-02-11 | — |
| TASK-008 | OTA recovery docs | MERGED DONE | 2026-02-11 | Eero |
| TASK-009 | GitHub Actions CI | MERGED DONE | 2026-02-11 | Oliver + Eero |
| TASK-010 | Lambda tests + linting CI | MERGED DONE | 2026-02-11 | Oliver + Eero |
| TASK-011 | Provisioning docs | MERGED DONE | 2026-02-11 | Eero |
| TASK-012 | MOER threshold validation | MERGED DONE | 2026-02-11 | Eero |
| TASK-013 | OTA field reliability | DEFERRED | 2026-02-13 | — |
| TASK-014 | Product Requirements Document | MERGED DONE | 2026-02-11 | — |
| TASK-015 | Remove dead sid_demo_parser | MERGED DONE | 2026-02-11 | Claude |
| TASK-016 | Architecture docs | MERGED DONE | 2026-02-11 | Eero |
| TASK-018 | Grenning tests in CI | MERGED DONE | 2026-02-11 | Eero |
| TASK-019 | clang-format CI | DECLINED | 2026-02-11 | — |
| TASK-020 | E2E runbook execution | MERGED DONE | 2026-02-11 | Eero |
| TASK-021 | Remove legacy rak1901_demo | MERGED DONE | 2026-02-11 | — |
| TASK-023 | PSA crypto -149 bug | MERGED DONE | 2026-02-11 | Claude + Eero |
| TASK-024 | API version mismatch hard stop | MERGED DONE | 2026-02-11 | Claude |
| TASK-025 | OTA chunk + delta bitmap tests | MERGED DONE | 2026-02-11 | Eero |
| TASK-027 | Shell command dispatch tests | MERGED DONE | 2026-02-11 | Eero |
| TASK-028 | MFG key health check tests | MERGED DONE | 2026-02-11 | Eero |
| TASK-031 | OTA image signing (ED25519) | MERGED DONE | 2026-02-14 | Eliel |
| TASK-033 | TIME_SYNC downlink (0x30) | MERGED DONE | 2026-02-14 | Eliel |
| TASK-034 | Event buffer (ring buffer) | MERGED DONE | 2026-02-14 | Eliel |
| TASK-036 | Device registry (DynamoDB) | MERGED DONE | 2026-02-14 | Eliel |
| TASK-039 | Commissioning self-test | MERGED DONE | 2026-02-14 | Eero |
| TASK-035 | Uplink payload v0x07 | MERGED DONE | 2026-02-14 | Eliel |
| TASK-040 | Production self-test trigger | MERGED DONE | 2026-02-14 | Eero |
| TASK-041 | Commissioning checklist card | MERGED DONE | 2026-02-14 | Bobby |
| TASK-043 | Warranty/liability risk | MERGED DONE | 2026-02-13 | Pam |
| TASK-050 | Delete platform-side EVSE shell files | MERGED DONE | 2026-02-15 | Eliel |
| TASK-051 | Move EVSE payload struct to app layer | MERGED DONE | 2026-02-15 | Eliel |
| TASK-052 | Rename rak_sidewalk → evse_payload | MERGED DONE | 2026-02-15 | Eliel |
| TASK-053 | Resolve two app_tx.c naming collision | MERGED DONE | 2026-02-15 | Eliel |
| TASK-044 | PRD update — commissioning + G = earth ground | MERGED DONE | 2026-02-16 | Pam |
| TASK-056 | Break up app.c into focused platform modules | MERGED DONE | 2026-02-16 | Eliel |
| TASK-065 | AC-priority software interlock + charge_block rename | MERGED DONE | 2026-02-17 | Eliel |
| TASK-068 | charge_block rename propagation + stale status updates | MERGED DONE | 2026-02-17 | Pam |
| TASK-062 | Wire up Charge Now button GPIO end-to-end | MERGED DONE | 2026-02-17 | Eliel |
| TASK-022 | BUG: Stale flash inflates OTA delta baselines | MERGED DONE | 2026-02-17 | Eero |
| TASK-048 | On-device selftest verification | MERGED DONE | 2026-02-17 | Eero |

## Related Documents
- RAK Firmware Technical Design: `docs/technical-design.md`
- Experiment Log: `experiment-log.md`
- MOER Threshold Analysis: `moer-threshold-analysis.md`
- OTA Field Test Results: `ota-field-test-results.md`
- ADRs: `docs/adr/`
- Known Issues: `docs/known-issues.md`
