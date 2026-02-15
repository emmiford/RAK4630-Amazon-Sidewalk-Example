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
| ID | P | Status | Owner | Title | Blocked By |
|----|---|--------|-------|-------|------------|
| TASK-048 | P0 | not started | Eero | On-device selftest verification | — |
| TASK-049 | P0 | not started | Eliel | Deploy device registry | — |
| TASK-022 | P1 | not started | — | BUG: Stale flash inflates OTA delta baselines | — |
| TASK-029 | P1 | not started | Eliel | Production observability | — |
| TASK-044 | P1 | not started | Pam | PRD update — commissioning + G = earth ground | — |
| TASK-045 | P1 | not started | Eliel | ED25519 verify library integration | — |
| TASK-046 | P1 | not started | Eero | Signed OTA E2E verification | TASK-045 |
| TASK-047 | P1 | not started | — | On-device verification (TIME_SYNC + buffer + v0x07) | — |
| TASK-001 | P2 | not started | Oliver | Merge feature/generic-platform to main | — |
| TASK-026 | P2 | not started | Eero | Boot path + app discovery tests | — |
| TASK-030 | P2 | not started | Eliel | Fleet command throttling | — |
| TASK-032 | P2 | not started | Eliel | Cloud command authentication | — |
| TASK-037 | P2 | planned | Pam | Utility identification (PRD scoping done) | — |
| TASK-038 | P2 | not started | Pam | Data privacy — policy + retention + CCPA | — |
| TASK-042 | P2 | not started | Pam | Privacy agent | — |
| TASK-048b | P2 | not started | Bobby | Charge Now single-press button | — |
| TASK-049b | P3 | not started | Eliel | Platform button callback (GPIO interrupt) | — |

## Completed Tasks (33)
| ID | Title | Status | Date | Agent |
|----|-------|--------|------|-------|
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

## Related Documents
- Experiment Log: `experiment-log.md`
- MOER Threshold Analysis: `moer-threshold-analysis.md`
- OTA Field Test Results: `ota-field-test-results.md`
- ADRs: `docs/adr/`
- Known Issues: `docs/known-issues.md`
