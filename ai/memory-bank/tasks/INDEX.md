# Task Index

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

## Open Tasks (25)

### P1 — Required for v1.0
| ID | Status | Owner | Title | Blocked By |
|----|--------|-------|-------|------------|
| TASK-102 | not started | — | Reseat RAK4631 in RAK19001 and validate all even-row pins | — |

### P2 — Important but not blocking v1.0
| ID | Status | Owner | Title | Blocked By |
|----|--------|-------|-------|------------|
| TASK-030 | not started | Eliel | Fleet command safety — design investigation | — |
| TASK-072 | partial pass | Eero | On-device Charge Now button GPIO verification | — |
| TASK-085 | not started | Eero | E2E Charge Now cloud opt-out verification | TASK-072 |
| TASK-086 | not started | Eliel | Terraform: add CMD_AUTH_KEY to scheduler Lambda | — |
| TASK-087 | not started | Eliel | Generate + provision production auth key | TASK-086 |
| TASK-088 | not started | Eero | Scheduler integration tests for signed payloads | — |
| TASK-093 | not started | Eliel | Clean up stale IoT rules and old Lambdas | — |
| TASK-095 | not started | — | PCB design update — 3 relays, 11 terminals, inline interlock | TASK-100 |
| TASK-096 | not started | Eliel | Firmware — W-out relay GPIO + platform API update | TASK-095 |
| TASK-097 | not started | Eliel | Firmware v1.1 — Heat call input + interlock + uplink HEAT flag | TASK-096 |
| TASK-098 | not started | Eliel | Cloud v1.1 — Decode HEAT flag in Lambda + DynamoDB | TASK-097 |
| TASK-099 | not started | Bobby | Commissioning checklist update for inline pass-through wiring | TASK-100 |
| TASK-103 | not started | — | Validate external button and potentiometer hardware independently | TASK-102 |

### P3 — Nice-to-have
| ID | Status | Owner | Title | Blocked By |
|----|--------|-------|-------|------------|
| TASK-049b | not started | Eliel | Platform button callback (GPIO interrupt) | — |
| TASK-074 | not started | Eliel | Device registry GSI for fleet-scale health queries | — |
| TASK-076 | not started | Pam | Engage external privacy consultant | — |
| TASK-077 | not started | Eliel | Implement deletion Lambda (PII + telemetry cleanup) | — |
| TASK-079 | not started | Pam | Consumer privacy request intake form | — |
| TASK-080 | not started | Pam | Publish privacy policy (web hosting) | TASK-076 |
| TASK-081 | not started | Pam | Formal incident response plan | TASK-076 |
| TASK-082 | not started | Eliel | Geolocation opt-out mechanism | TASK-076 |
| TASK-083 | not started | Eliel | Automate data export for Right to Know | — |
| TASK-089 | in progress | Eliel | Update technical-design.md for v0x09 + event buffer drain | — |
| TASK-104 | not started | — | Evaluate SAADC errata workaround retention | — |

## Completed Tasks (82)
| ID | Title | Status | Date | Agent |
|----|-------|--------|------|-------|
| TASK-107 | Device timestamp as DynamoDB sort key (ADR-007) | MERGED DONE | 2026-02-22 | Eliel |
| TASK-106 | EVSE Fleet Dashboard + Table Migration (ADR-006) | MERGED DONE | 2026-02-22 | Eliel |
| TASK-105 | Naming consistency audit and cleanup | MERGED DONE | 2026-02-21 | Utz |
| TASK-101 | Build version tracking + release tooling | MERGED DONE | 2026-02-21 | Eliel+Pam+Utz |
| TASK-100 | Remap firmware pins to RAK19007 WisBlock connector | MERGED DONE | 2026-02-19 | Eliel |
| TASK-094 | Merge PRD + TDD inline wiring doc updates | MERGED DONE | 2026-02-19 | Pam+Utz |
| TASK-065 | AC-priority software interlock + charge_block rename | MERGED DONE | 2026-02-19 | Eliel |
| TASK-032 | Cloud command authentication (HMAC-SHA256) | MERGED DONE | 2026-02-19 | Eliel |
| TASK-069 | Interlock transition event logging | MERGED DONE | 2026-02-19 | Eliel |
| TASK-061 | Event buffer — write on state change | MERGED DONE | 2026-02-19 | Eliel |
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
| TASK-023 | PSA crypto -149 bug | MERGED DONE | 2026-02-17 | Eliel (verified: Eero) |
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
| TASK-068 | charge_block rename propagation + stale status updates | MERGED DONE | 2026-02-17 | Pam |
| TASK-062 | Wire up Charge Now button GPIO end-to-end | MERGED DONE | 2026-02-17 | Eliel |
| TASK-022 | BUG: Stale flash inflates OTA delta baselines | MERGED DONE | 2026-02-17 | Eero |
| TASK-048 | On-device selftest verification | MERGED DONE | 2026-02-17 | Eero |
| TASK-029 | Production observability (health digest + diagnostics) | MERGED DONE | 2026-02-17 | Eliel |
| TASK-049 | Deploy device registry (terraform + physical verify) | MERGED DONE | 2026-02-17 | Eliel |
| TASK-060 | Uplink payload v0x08 — reserve heat flag, keep pilot voltage | MERGED DONE | 2026-02-17 | Eliel |
| TASK-045 | ED25519 verify library integration | MERGED DONE | 2026-02-17 | Eliel |
| TASK-066 | Button re-test clears FAULT_SELFTEST on all-pass | MERGED DONE | 2026-02-17 | Eliel |
| TASK-067 | LED blink priority state machine (PRD §2.5.1) | MERGED DONE | 2026-02-17 | Eliel |
| TASK-058 | On-device shell verification (post app.c refactor) | MERGED DONE | 2026-02-17 | Eero |
| TASK-063 | Delay window support (device + cloud) | MERGED DONE | 2026-02-17 | Eliel |
| TASK-037 | Utility identification (PRD scoping done) | MERGED DONE | 2026-02-17 | Pam |
| TASK-038 | Data privacy — policy + retention + CCPA | MERGED DONE | 2026-02-17 | Pam |
| TASK-042 | Privacy agent | MERGED DONE | 2026-02-17 | Pam |
| TASK-047 | On-device verification (TIME_SYNC + buffer + v0x08) | MERGED DONE | 2026-02-17 | Eero |
| TASK-048b | Charge Now 30-min latch (ADR-003) | MERGED DONE | 2026-02-17 | Eliel |
| TASK-071 | Scheduler sentinel divergence detection | MERGED DONE | 2026-02-17 | Eliel |
| TASK-073 | Automate remote diagnostic queries from health digest | MERGED DONE | 2026-02-17 | Eliel |
| TASK-046 | Signed OTA E2E verification | MERGED DONE | 2026-02-18 | Eero |
| TASK-075 | On-device delay window verification | MERGED DONE | 2026-02-18 | Eero |
| TASK-055 | Split ota_update.c → ota_flash.c + ota_update.c | MERGED DONE | 2026-02-18 | Eliel |
| TASK-057 | Route selftest through evse_sensors, not direct ADC | MERGED DONE | 2026-02-18 | Eero |
| TASK-064 | Cloud Charge Now protocol (ADR-003) | MERGED DONE | 2026-02-18 | Eliel |
| TASK-054 | Shared platform API pointer (replace 13 setters) | MERGED DONE | 2026-02-19 | Eliel |
| TASK-084 | Populate registry app_version from diagnostics responses | MERGED DONE | 2026-02-19 | Eliel |
| TASK-070 | Production heartbeat interval (60s → 15min) | MERGED DONE | 2026-02-19 | Eliel |
| TASK-026 | Boot path + app discovery tests (10 tests) | MERGED DONE | 2026-02-19 | Eero |
| TASK-090 | Codebase streamlining — test consolidation, LOG macros, DRY constants | MERGED DONE | 2026-02-19 | Eliel+Utz+Eero |
| TASK-078 | Implement daily aggregation Lambda | MERGED DONE | 2026-02-19 | Eliel |
| TASK-091 | Documentation sync — PRD, TDD, commissioning card, project plan | MERGED DONE | 2026-02-19 | Pam+Eliel+Utz |
| TASK-092 | BUG: J1772 state enum mismatch between firmware and Lambda | MERGED DONE | 2026-02-19 | Eliel |

## Related Documents
- RAK Firmware Technical Design: `docs/technical-design.md`
- Experiment Log: `experiment-log.md`
- MOER Threshold Analysis: `moer-threshold-analysis.md`
- OTA Field Test Results: `ota-field-test-results.md`
- ADRs: `docs/adr/`
- Known Issues: `docs/known-issues.md`
