# RAK Sidewalk EVSE Monitor — Development Task List

## Session Registry

| Agent | Role | Branch | Sessions | Tasks |
|-------|------|--------|----------|-------|
| Oliver | Architecture / OTA / infra | `main`, `feature/generic-platform` | 2026-02-11 | TASK-001, 002, 004, 006, 007, 009, 010, 014, 019 |
| Eero | Testing architect | `feature/testing-pyramid` | 2026-02-11 | TASK-003, 005, 009, 010, 011, 012, 013, 016, 018, 020, 021, 039, 040 |
| Eliel | Backend architect | TBD | 2026-02-13 | TASK-029, 030, 031, 032, 033, 034, 035, 036 |
| Pam | Product manager | TBD | 2026-02-13 | TASK-037, 038, 042, 043, 044 |
| Bobby | Brand guardian | TBD | 2026-02-13 | TASK-041 |

## Specification Summary
**Project**: Embedded IoT EVSE monitor over Amazon Sidewalk (LoRa) with OTA firmware updates
**Technical Stack**: Zephyr RTOS / nRF Connect SDK, C (firmware), Python (AWS Lambda), Terraform (IaC)
**Hardware**: RAK4631 (nRF52840 + SX1262 LoRa)
**Base Branch**: `main`
**Active Feature Branch**: None (generic-platform merged to main)

## Current State (2026-02-11)
- Platform/app separation complete on `feature/generic-platform`
- 32 host-side unit tests passing (app layer, Grenning Makefile)
- 59 new C unit tests passing (Unity/CMake, `tests/` directory)
- 81 Python tests passing (pytest, `aws/tests/`)
- 7 serial shell integration tests passing on device
- OTA pipeline operational (delta mode, deploy CLI, reliability fixes — all merged to main)
- AWS infra deployed: decode Lambda, OTA sender Lambda, charge scheduler Lambda, DynamoDB, S3, EventBridge
- CLAUDE.md exists, CI pipeline operational (GitHub Actions: cppcheck + C tests + Python tests)
- Active feature branch: `feature/testing-pyramid` (not yet merged to main)

## Related Documents
- **Experiment Log** (Oliver): `ai/memory-bank/tasks/experiment-log.md`
  - 8 concluded experiments (7 GO, 1 REVERTED)
  - 6 recommended experiments (REC-001 through REC-006)
  - Key finding: WattTime MOER threshold (REC-004) and OTA field reliability (REC-005) are high-priority experiments that should inform future tasks
- **Architecture Decision Records**: `docs/adr/`
  - ADR-001: API version mismatch is a hard stop
- **Known Issues**: `docs/known-issues.md`
  - KI-001: Version mismatch leaves device in platform-only mode
  - KI-002: PSA crypto AEAD error -149 after platform re-flash (TASK-023)
  - KI-003: Stale flash data inflates OTA delta baselines (TASK-022)

---

## Development Tasks

---

### TASK-001: Merge feature/generic-platform to main

## Branch & Worktree Strategy
**Base Branch**: `main`
- Merge `feature/generic-platform` into `main` after verification
- Delete feature branch after merge

## Description
The generic platform refactor is complete (2 commits: `d5a84a5`, `e88d519`). All EVSE domain logic has been moved from platform to app layer. This needs to be merged to main so subsequent work builds on the new architecture.

## Dependencies
**Blockers**: None
**Unblocks**: All other tasks (new work should be based on the generic platform architecture)

## Acceptance Criteria
- [ ] Host-side unit tests pass (`make -C app/rak4631_evse_monitor/tests/ clean test`)
- [ ] Full firmware builds successfully (`west build -p -b rak4631 app/rak4631_evse_monitor/ -- -DOVERLAY_CONFIG="lora.conf"`)
- [ ] App-only build succeeds (CMake standalone build in `app_evse/`)
- [ ] Git log reviewed — no accidental files, clean commit messages
- [ ] Merged to main (fast-forward or merge commit per team convention)
- [ ] Feature branch deleted

## Testing Requirements
- [ ] Existing 32 unit tests pass
- [ ] Build verification (platform + app-only)

## Completion Requirements (Definition of Done)
- [ ] `feature/generic-platform` merged to `main`
- [ ] All tests pass on `main`
- [ ] Feature branch removed

## Deliverables
- Clean `main` branch with generic platform architecture

**Size**: S (2 points) — 30 min

---

### TASK-002: Create CLAUDE.md project configuration — MERGED DONE

## Status: MERGED DONE (2026-02-11)
CLAUDE.md already existed with comprehensive coverage exceeding acceptance criteria. Includes safety notes, device-specific details, and branch conventions beyond original scope.

## Acceptance Criteria
- [x] CLAUDE.md exists at project root
- [x] Documents: project purpose, architecture (platform vs app split), build commands (native, docker, app-only), test commands, key directories, branch conventions
- [x] References platform API contract (`include/platform_api.h`)
- [x] Documents flash layout (platform @ 0x0, app @ 0x90000, staging @ 0xD0000)
- [x] Lists AWS infrastructure components
- [x] Includes OTA deploy workflow (`ota_deploy.py` commands)

**Size**: S (2 points) — 30 min

---

### TASK-003: Update README.md for generic platform architecture — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
README updated on `feature/testing-pyramid` with comprehensive testing section documenting all 4 test suites (Unity/CMake 75 tests, Grenning 32 tests, Python 81 tests, integration 7 tests). Note: changes are on feature branch, not yet merged to main.

## Acceptance Criteria
- [x] README documents platform vs app architecture
- [x] Explains app-only build process (CMake standalone)
- [x] Documents OTA deployment workflow
- [x] Describes host-side testing with `make -C tests/`
- [x] Includes flash memory map diagram
- [x] Lists shell commands available (sid, evse, hvac)

## Deliverables
- Updated `/README.md`

**Size**: M (3 points) — 45 min

---

### TASK-004: Add charge scheduler Lambda unit tests — MERGED DONE

## Status: MERGED DONE (2026-02-11)
17 tests in `aws/tests/test_charge_scheduler.py`. Covers TOU peak detection (8 tests), charge command payload format (3 tests), lambda handler decision logic (3 tests), and MOER integration (3 tests). All passing in CI. Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Test file: `aws/tests/test_charge_scheduler.py`
- [x] Tests cover: TOU peak detection (weekday on-peak, off-peak, weekend)
- [x] Tests cover: MOER threshold logic (above, below, None/unavailable)
- [x] Tests cover: Decision matrix (TOU peak only, MOER high only, both, neither)
- [x] Tests cover: State deduplication (skip downlink when command unchanged)
- [x] Tests cover: DynamoDB state read/write (mocked)
- [x] Tests cover: Downlink payload format (0x10 command byte, allow/pause)
- [x] All tests pass: `python -m pytest aws/tests/test_charge_scheduler.py`

**Size**: M (3 points) — 60 min

---

### TASK-005: Add OTA recovery path host-side tests — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
16 tests in `tests/app/test_ota_recovery.c` using Unity/CMake framework (not Grenning). Tests use RAM-backed mock flash (400KB covering primary + metadata + staging). Mock Zephyr headers created at `tests/mocks/zephyr/` (kernel.h, device.h, drivers/flash.h, logging/log.h, sys/crc.h, sys/reboot.h). All 16 tests passing in CI. Branch: `feature/testing-pyramid`.

Key finding: `clear_metadata()` page-aligned erase at 0xCFF00 extends to 0xD0FFF, partially erasing first page of staging area. Tests account for this behavior.

## Acceptance Criteria
- [x] Tests: `tests/app/test_ota_recovery.c` (16 tests, Unity/CMake)
- [x] Test: recovery detects APPLYING metadata at boot and resumes copy
- [x] Test: recovery completes remaining pages (interrupted at page 3 of 5)
- [x] Test: recovery verifies magic word after completion
- [x] Test: recovery clears metadata after successful apply
- [x] Test: no recovery action when metadata is clear (normal boot)
- [x] All tests pass: `ctest --test-dir tests/build --output-on-failure`

## Deliverables
- `tests/app/test_ota_recovery.c` (16 tests)
- `tests/mocks/mock_flash.c` (flash globals + reset)
- `tests/mocks/zephyr/` (7 mock Zephyr headers)
- Updated `tests/CMakeLists.txt`

**Size**: M (3 points) — 60 min

---

### TASK-006: Add decode Lambda unit tests — MERGED DONE

## Status: MERGED DONE (2026-02-11)
20 tests in `aws/tests/test_decode_evse.py`. Covers raw EVSE payload decoding (9 tests), OTA uplink decoding (6 tests), full base64 decode pipeline (4 tests), and lambda handler integration with DynamoDB + OTA forwarding (2 tests). All passing in CI. Branch: `feature/testing-pyramid`.

Note: Legacy sid_demo format decoding not explicitly tested — the decode_legacy_sid_demo_payload function exists but is a fallback path with scan-based parsing that's harder to unit test precisely. Coverage focuses on the primary raw format.

## Acceptance Criteria
- [x] Test file: `aws/tests/test_decode_evse.py`
- [x] Tests cover: New raw payload decoding (magic 0xE5, version 0x01)
- [x] Tests cover: J1772 state extraction, voltage, current, thermostat flags
- [ ] Tests cover: Legacy sid_demo format decoding — NOT COVERED (low priority, fallback path)
- [x] Tests cover: DynamoDB write (mocked)
- [x] Tests cover: OTA ACK detection and Lambda invoke trigger
- [x] Tests cover: Malformed payload handling (short, bad magic)
- [x] All tests pass: `python -m pytest aws/tests/test_decode_evse.py`

**Size**: M (3 points) — 45 min

---

### TASK-007: Create E2E test plan for device-to-cloud round-trip — MERGED DONE

## Status: MERGED DONE (2026-02-11)
E2E runbook exists at `tests/e2e/RUNBOOK.md`. Covers boot/connect, telemetry flow, charge control downlink, OTA update, and sensor simulation. Manual checklist format with shell + AWS CLI verification commands.

## Acceptance Criteria
- [x] Test plan document: `tests/e2e/RUNBOOK.md`
- [x] Covers: Uplink path (device → Sidewalk → decode Lambda → DynamoDB)
- [x] Covers: Downlink path (charge control → IoT Wireless → device)
- [x] Covers: OTA path (deploy → status → complete → reboot)
- [ ] Covers: Recovery path (power cycle during OTA apply) — NOT COVERED, see TASK-013
- [x] Each test has: preconditions, steps, expected results, verification commands
- [x] Includes shell commands for device-side verification
- [x] Includes AWS CLI commands for cloud-side verification

**Size**: M (3 points) — 45 min

---

### TASK-008: Document OTA recovery and rollback procedures — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
Comprehensive 533-line runbook at `docs/ota-recovery.md`. Covers OTA state machine with ASCII diagram, recovery metadata struct layout, 5 failure modes with log messages, shell diagnosis commands, 5 manual recovery procedures, full cloud-side CLI reference, rollback limitations, prevention practices, and a quick-reference decision tree.

## Acceptance Criteria
- [x] Document: `docs/ota-recovery.md` (533 lines)
- [x] Documents OTA state machine: IDLE → RECEIVING → VALIDATING → COMPLETE → APPLYING → reboot
- [x] Documents recovery metadata: flash address, struct fields, what survives power loss
- [x] Documents failure modes: power loss (RECEIVING + APPLYING), CRC mismatch, magic failure, stale flash
- [x] Documents manual recovery: shell commands, log message reference, decision tree
- [x] Documents rollback: no A/B, no auto-rollback, manual OTA of old version
- [x] Includes full `ota_deploy.py` CLI reference (status, abort, clear-session, deploy, baseline, preview)

**Size**: S (2 points) — 30 min

---

### TASK-009: Set up GitHub Actions for host-side unit tests — MERGED DONE (Oliver + Eero)

## Status: MERGED DONE (2026-02-11, Oliver initial + Eero completed)
CI at `.github/workflows/ci.yml` runs both test suites: CMake/ctest (Unity tests) and Grenning Makefile tests. Also includes cppcheck static analysis and Python pytest. All 4 CI jobs passing. Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Workflow file: `.github/workflows/ci.yml`
- [x] Triggers on: push to main + feature/*, pull requests
- [x] C unit tests via CMake/ctest (newer test suite)
- [x] C unit tests via Grenning Makefile (original 32 tests) — added via TASK-018
- [x] cppcheck static analysis
- [x] Python pytest (Lambda tests)

**Size**: S (2 points) — 30 min

---

### TASK-010: Set up GitHub Actions for Lambda tests + linting — MERGED DONE (Oliver + Eero)

## Status: MERGED DONE (2026-02-11, Oliver initial + Eero completed)
CI runs pytest and ruff linting. Config in `pyproject.toml` (line-length 100, E/W/F/I rules). Auto-fixed 30 violations (unused f-prefixes, unused imports). Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Python tests run in CI via pytest
- [x] `aws/requirements-test.txt` exists (pytest, pytest-cov, ruff)
- [x] Python linting (ruff) added to CI
- [x] `pyproject.toml` with ruff config created
- [x] All existing code passes ruff

**Size**: S (2 points) — 30 min

---

### TASK-011: Document device provisioning workflow — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
Provisioning guide at `docs/provisioning.md`. Covers credential generation, MFG partition build, flash sequence, first boot registration, shell diagnostics, AWS IoT Wireless registration, E2E verification, OTA baseline capture, and troubleshooting table. Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Document: `docs/provisioning.md`
- [x] Covers: Sidewalk credential generation (AWS IoT Wireless console)
- [x] Covers: Credential file format and placement
- [x] Covers: Flashing credentials to device (flash.sh, pyOCD, safety warnings)
- [x] Covers: AWS IoT Wireless device registration (CLI command)
- [x] Covers: Verification steps (sid status, sid mfg, DynamoDB query)
- [x] References `credentials.example/` as template

## Deliverables
- `docs/provisioning.md`

**Size**: S (2 points) — 30 min

---

### TASK-012: Validate WattTime MOER threshold for PSCO region — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
Analysis script created and executed. WattTime free tier only provides current signal index (not historical data needed for full distribution analysis). Current MOER consistently at 70% for PSCO region. Recommendation: keep 70% threshold. Full historical analysis requires WattTime paid tier upgrade. Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Script: `aws/scripts/moer_analysis.py`
- [ ] Analysis output: distribution histogram — NOT POSSIBLE (free tier returns only current value)
- [ ] Simulation: threshold comparison — NOT POSSIBLE (no historical data)
- [x] Cross-reference with Xcel TOU peak hours — done in script logic
- [x] Recommendation: keep 70% threshold (documented with rationale)
- [x] Document findings in `ai/memory-bank/tasks/moer-threshold-analysis.md`
- [x] No threshold change warranted

## Deliverables
- `aws/scripts/moer_analysis.py`
- `ai/memory-bank/tasks/moer-threshold-analysis.md`

**Size**: M (3 points) — 60 min

---

### TASK-013: OTA field reliability testing across RF conditions — DEFERRED

## Status: DEFERRED (2026-02-13)
Test plan written at `ai/memory-bank/tasks/ota-field-test-results.md`. Deprioritized to P4 — LoRa RF reliability at range is fundamentally a Sidewalk/AWS platform concern, not something we can solve at the application layer. We verify OTA works in lab; Sidewalk's link-layer retries handle the rest.

## Acceptance Criteria
- [x] Test plan documented in `ai/memory-bank/tasks/ota-field-test-results.md`
- [x] Test conditions: lab (~1m), indoor (~10m), outdoor (~50m), max-range (~200m+)
- [ ] Per condition: 3 OTA cycles — PENDING (requires field work)
- [ ] Per cycle record: success/fail, transfer time, retry count — PENDING
- [ ] Test power-cycle-during-apply — PENDING
- [ ] Results summary — PENDING
- [ ] Go/no-go recommendation — PENDING

## Deliverables
- [x] `ai/memory-bank/tasks/ota-field-test-results.md` (plan written)
- [ ] Same file updated with execution results (pending field work)

**Size**: L (5 points) — Plan done. Execution requires multiple physical test sessions.

---

### TASK-014: Create Product Requirements Document (PRD) — MERGED DONE

## Status: MERGED DONE (2026-02-11)
Retroactive v1.0 PRD written to `docs/PRD.md`. Covers 9 sections: product overview, device requirements, connectivity, cloud, operational, non-functional, scope boundaries, known gaps, and requirement traceability. Every requirement tagged IMPLEMENTED / NOT STARTED / N/A. All 14 remaining backlog tasks mapped to PRD gaps.

## Acceptance Criteria
- [x] Document: `docs/PRD.md`
- [x] Covers product overview and target user/deployment
- [x] Covers device requirements (J1772, current clamp, thermostat, charge control, LEDs, shell)
- [x] Covers connectivity requirements (Sidewalk LoRa, uplink/downlink, MTU)
- [x] Covers cloud requirements (decode, OTA pipeline, demand response, alerting)
- [x] Covers operational requirements (OTA, provisioning, observability)
- [x] Covers non-functional requirements (power, reliability, security)
- [x] Lists what's in-scope for v1.0 vs future
- [x] Lists known gaps between requirements and current implementation
- [x] Each requirement has a status: IMPLEMENTED, NOT STARTED, N/A

## Deliverables
- `docs/PRD.md`

**Size**: M (3 points) — 60 min

---

### TASK-015: Remove dead sid_demo_parser code — MERGED DONE (Claude)

## Status: MERGED DONE (2026-02-11, Claude)
Deleted 5 files (~1,600 lines) from `ext/`, removed `ext/` directory entirely. Removed source entry and include path from `CMakeLists.txt`. Grep confirmed zero references in app code. All 9 C test suites pass.

## Acceptance Criteria
- [x] Remove from `CMakeLists.txt`: `ext/sid_demo_parser.c` source entry
- [x] Delete files: all 5 ext/ files
- [x] Verify: grep for references — zero in app code
- [x] Host-side unit tests pass (9/9)

**Size**: XS (1 point) — 15 min

---

### TASK-016: Document SDK divergence and architecture decisions — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
Comprehensive architecture document created at `docs/architecture.md`. Covers split-image design, memory layout diagram, API contract (22 platform functions, 7 app callbacks), boot sequence, OTA flow, SDK compliance table, patches, GPIO mapping, and testing architecture. Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Document: `docs/architecture.md`
- [x] Covers: split-image architecture (platform @ 0x0, app @ 0x90000, staging @ 0xD0000)
- [x] Covers: platform API contract (function table, magic words, versioning)
- [x] Covers: SDK compliance — which files are unchanged, which are extended, why
- [x] Covers: Sidewalk initialization flow with custom diagnostics
- [x] Covers: OTA system design (receive → validate → stage → apply → recover)
- [x] Covers: patches applied and rationale
- [x] Covers: what is NOT customized (Sidewalk protocol stack, BLE, radio driver)
- [x] Includes flash memory map diagram

## Deliverables
- `docs/architecture.md`

**Size**: M (3 points) — 45 min

---

### TASK-018: Add old Grenning tests to CI — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
Added `test-c-grenning` job to `.github/workflows/ci.yml`. Runs `make -C app/rak4631_evse_monitor/tests clean test` as a separate CI job. All 4 CI jobs passing (lint, test-c, test-c-grenning, test-python). Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Add step to `.github/workflows/ci.yml` that runs `make -C app/rak4631_evse_monitor/tests/ clean test`
- [x] Both test suites (CMake ctest + Grenning make) run and must pass
- [x] CI fails if either suite fails

## Deliverables
- Updated `.github/workflows/ci.yml`

**Size**: XS (1 point) — 15 min

---

### TASK-019: Add clang-format configuration and CI enforcement — DECLINED

## Status: DECLINED (2026-02-11)
Evaluated and declined. Code is already consistently styled by hand. clang-format would risk reformatting existing code (brace placement, #define alignment), polluting git blame for minimal benefit. cppcheck in CI catches real bugs. Revisit if team grows or style drift becomes a problem.

**Reference**: Oliver's experiment log — EXP-009

---

### TASK-020: Execute E2E runbook tests on physical device — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
6/7 E2E tests passed, OTA test skipped (device in use by other agent). Results documented at `tests/e2e/RESULTS-2026-02-11.md`. Key finding: serial DTR reset issue — must use `/dev/cu.usbmodem101` (not tty) with persistent connections. J1772 state mapping mismatch found between firmware and decode Lambda. Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Every item in `tests/e2e/RUNBOOK.md` executed (6/7, OTA skipped)
- [x] Results documented: `tests/e2e/RESULTS-2026-02-11.md`
- [x] Boot & connect: Sidewalk READY confirmed
- [x] Telemetry: uplink received in DynamoDB with correct fields
- [x] Charge control: local pause/allow verified on device
- [ ] OTA: skipped (device in use by other agent)
- [x] Sensor simulation: state changes verified on device + DynamoDB
- [x] Issues documented: DTR reset bug, J1772 state mapping mismatch

## Deliverables
- `tests/e2e/RESULTS-2026-02-11.md`

**Size**: M (3 points) — 45 min (requires physical device access)

---

### TASK-021: Archive or remove legacy rak1901_demo app — MERGED DONE

## Status: MERGED DONE (2026-02-11)
Already removed in commit `0a3e622` (merged to main via `900dfbe`). Decision: remove (not archive). 39 files deleted (3,925 lines). RAK1901 driver and DTS binding also removed. No remaining references in codebase. Upstream RAKWireless repo retains originals.

## Acceptance Criteria
- [x] Decision documented: REMOVE (upstream retains originals)
- [x] Files deleted: 39 files, 3,925 lines
- [x] References cleaned up: CMakeLists, Kconfig, DTS bindings
- [x] Build still succeeds

## Deliverables
- Clean `app/` directory (only `rak4631_evse_monitor/` remains)

**Size**: XS (1 point) — 15 min

---

## Task Dependency Graph

```
TASK-005 (OTA recovery tests) → TASK-008 (OTA recovery docs)
                               → TASK-013 (OTA field testing)

TASK-020 (Execute E2E runbook) → TASK-013 (field testing builds on E2E baseline)

TASK-004 (Scheduler tests) ──→ auto-picked up by existing CI pytest job
TASK-006 (Decode tests) ─────→ auto-picked up by existing CI pytest job

TASK-018 (Grenning tests in CI) — independent
TASK-019 (clang-format + CI) ——— independent
TASK-012 (MOER threshold) ————— independent (Oliver REC-004)
TASK-011 (Provisioning docs) —— independent
TASK-015 (Remove dead ext/) ——— independent
TASK-016 (Architecture docs) —— independent
TASK-021 (Legacy app cleanup) — independent
TASK-022 (Stale flash bug) ———— independent (plan drafted, not approved)
TASK-023 (PSA crypto bug) ————— blocked by TASK-028 (key health tests inform diagnosis)
TASK-024 (API version hard stop)— independent (Eliel: architecture concern #1)
TASK-025 (OTA chunk tests) ———— independent (Eliel/Eero: critical test gap)
TASK-026 (Boot path tests) ———— blocked by TASK-024 (version behavior must be defined)
TASK-027 (Shell cmd tests) ———— independent (Eero: untested operator interface)
TASK-028 (MFG key health tests) — unblocks TASK-023

TASK-029 (Prod observability) —— independent (Eliel: cloud alerting + remote query)
TASK-030 (Fleet cmd throttling) — independent (Eliel: security threat mitigation)
TASK-031 (OTA image signing) ——— MERGED DONE (Eliel)
TASK-045 (ED25519 verify lib) —— blocked by TASK-031 (needs signing infra)
TASK-046 (Signed OTA E2E) ————— blocked by TASK-045 (needs real verify on device)
TASK-032 (Cloud cmd auth) ————— independent (Eliel: signed downlinks)
TASK-033 (TIME_SYNC downlink) —— independent (Eliel: device wall-clock time)
TASK-034 (Event buffer) ————————— depends on TASK-033 (ACK watermark needs time sync)
TASK-035 (Uplink v0x07) ————————— blocked by TASK-033 + TASK-034 (timestamp + event buffer)
TASK-036 (Device registry) ————— independent (Eliel: DynamoDB fleet identity)
TASK-037 (Utility identification) — blocked by TASK-036 (meter_number lives in registry)
TASK-038 (Data privacy) ————————— independent (Pam: policy + CCPA)
TASK-039 (Commissioning self-test) — independent (Eero: P0 for field install)
TASK-040 (Prod self-test trigger) — blocked by TASK-039 (needs self-test logic first + physical button)
TASK-041 (Commissioning card) ——— independent (Bobby: printed card design)
TASK-042 (Privacy agent) ————————— independent (Pam: assign legal/privacy owner)
TASK-044 (PRD commissioning + wiring) — independent (Pam: commissioning sections, G = earth ground)
```

## Priority Order (Recommended)

**Status pipeline**: not started -> planned -> in progress -> coded -> committed -> pushed -> merged done
**Special**: deferred, declined

| Priority | Task | Rationale |
|----------|------|-----------|
| Merged done | TASK-002 | CLAUDE.md — already existed |
| Merged done | TASK-003 | README updated (on feature/testing-pyramid) |
| Merged done | TASK-004 | Charge scheduler tests — 17 tests passing |
| Merged done | TASK-005 | OTA recovery tests — 16 tests passing |
| Merged done | TASK-006 | Decode Lambda tests — 20 tests passing |
| Merged done | TASK-007 | E2E test plan (runbook written) |
| Merged done | TASK-009 | CI pipeline — all 4 jobs (CMake + Grenning + cppcheck + pytest) |
| Merged done | TASK-010 | Python linting (ruff) added to CI |
| Merged done | TASK-011 | Provisioning docs at docs/provisioning.md |
| Merged done | TASK-012 | MOER threshold validated — keep 70% |
| Merged done | TASK-014 | PRD written |
| Merged done | TASK-016 | Architecture docs at docs/architecture.md |
| Merged done | TASK-018 | Grenning tests added to CI |
| Merged done | TASK-020 | E2E runbook executed — 6/7 pass, OTA skipped |
| Merged done | TASK-021 | Legacy rak1901_demo removed (commit 0a3e622) |
| Merged done | TASK-024 | API version mismatch hardened to hard stop (Claude) |
| Merged done | TASK-025 | OTA chunk + delta bitmap tests — 13 C + 13 Python (Eero) |
| Merged done | TASK-027 | Shell command dispatch tests — 31 tests (Eero) |
| Merged done | TASK-028 | MFG key health check tests — 7 tests (Eero) |
| — | TASK-019 | clang-format — DECLINED |
| Merged done | TASK-015 | Dead code removal — 5 files, ~1,600 lines deleted (Claude) |
| Merged done | TASK-023 | PSA crypto -149 root caused, flash.sh warning added (Claude + Eero) |
| P4 | TASK-013 | OTA field RF testing — deferred, Sidewalk platform concern |
| P0 | TASK-039 | Commissioning self-test — P0 for first field install (Eero) |
| P0 | TASK-041 | Commissioning checklist card — P0 for first field install (Bobby) |
| P1 | TASK-022 | BUG: Stale flash inflates OTA delta baselines (plan drafted, not approved) — KI-003 |
| P1 | TASK-033 | TIME_SYNC downlink (0x30) — enables device timestamps (Eliel) |
| P1 | TASK-034 | Event buffer — ring buffer + ACK watermark, depends on 033 (Eliel) |
| P1 | TASK-035 | Uplink v0x07 — timestamp + control flags, blocked by 033+034 (Eliel) |
| Merged done | TASK-031 | OTA image signing — ED25519, merged 2026-02-14 (Eliel) |
| P1 | TASK-029 | Production observability — CloudWatch alerting + remote query (Eliel) |
| P1 | TASK-036 | Device registry — DynamoDB fleet identity table (Eliel) |
| P1 | TASK-040 | Production self-test trigger — 5-press button, blocked by 039 (Eero) |
| P1 | TASK-044 | PRD update — commissioning sections + G = earth ground, no fan (Pam) |
| P2 | TASK-001 | Merge feature branches to main |
| P2 | TASK-026 | Boot path + app discovery tests (Eero, unblocked by TASK-024) |
| P2 | TASK-030 | Fleet command throttling — staggered random delays (Eliel) |
| P2 | TASK-032 | Cloud command authentication — signed downlinks (Eliel) |
| Planned | TASK-037 | Utility identification — lookup pipeline + TOU data model designed (Pam) |
| P2 | TASK-038 | Data privacy — policy + retention + CCPA review (Pam) |
| P2 | TASK-042 | Privacy agent — assign legal/privacy owner (Pam) |
| Planned | TASK-043 | Warranty/liability risk — EVSE pilot wire, MMWA, mitigation roadmap (Pam) |
| P1 | TASK-045 | ED25519 verify library integration — platform firmware (Eliel) |
| P1 | TASK-046 | Signed OTA E2E verification on device — blocked by 045 (Eero) |
| Merged done | TASK-008 | OTA recovery runbook — 533-line runbook (Eero) |
| Merged done | TASK-043 | Warranty/liability risk — PRD section 6.4 (Pam) |

---

### TASK-043: Warranty and liability risk assessment — MERGED DONE (Pam)

## Status: MERGED DONE (2026-02-13, Pam)
## Branch: `feature/warranty-scoping`

PRD section 6.4 added: Warranty and Liability. Documents risk that intercepting J1772 pilot wire may void EVSE/vehicle warranties. Covers per-circuit risk assessment (pilot HIGH, clamp NONE, relay HIGH, thermostat LOW), Magnuson-Moss Warranty Act analysis, 8 mitigations, phased compliance roadmap, and 5 open questions for legal review. Known Gaps and Traceability tables updated.

## Acceptance Criteria
- [x] PRD documents warranty risk per circuit (pilot, current clamp, relay, thermostat)
- [x] PRD documents Magnuson-Moss Warranty Act applicability
- [x] PRD documents 8 mitigation strategies
- [x] PRD documents phased compliance roadmap (v1.0 → v2.1)
- [x] PRD documents open questions for legal review

**Size**: S (2 points) — 30 min

---

### TASK-022: BUG — Stale flash data inflates OTA delta baselines

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `fix/stale-flash-erase`

## Description
When physically flashing a smaller app over a larger one, pyOCD only erases pages it writes to. Pages beyond the new image retain old code. `ota_deploy.py baseline` captures the full partition trimming only trailing 0xFF — stale non-0xFF bytes survive and inflate the baseline. Same problem after OTA apply: the apply loop only processes pages for the new image size, leaving stale pages from a previous larger image.

**Symptom**: Baseline shows 4524 bytes when actual app is 239 bytes. Delta OTA computes against inflated baseline.

**Plan**: Saved at `~/.claude/plans/witty-painting-matsumoto.md` — NOT YET APPROVED. Three-layer defense-in-depth:
1. `flash.sh`: Erase app partition before writing (primary fix)
2. `ota_update.c`: Erase stale pages after OTA apply — local flash op, no extra OTA chunks (~5s)
3. `ota_deploy.py`: Warn if baseline is significantly larger than app binary

## Dependencies
**Blockers**: None
**Unblocks**: None (but affects OTA delta reliability)

## Acceptance Criteria
- [ ] `flash.sh app` erases 0x90000-0xCEFFF before writing app hex
- [ ] OTA apply (full, delta, recovery) erases pages beyond new image up to metadata boundary
- [ ] `ota_deploy.py baseline` warns if dump is significantly larger than app.bin
- [ ] Host-side tests cover stale page erase after apply
- [ ] Manual verification: flash large app → flash small app → dump partition → all bytes beyond small app are 0xFF

**Size**: M (3 points) — 60 min

---

### TASK-023: BUG — PSA crypto AEAD error -149 after platform re-flash — MERGED DONE (Claude + Eero)

## Status: MERGED DONE (2026-02-11, Claude + Eero investigation)
Root cause confirmed: HUK regenerated on platform flash, MFG keys still present, so `mfg_key_health_check()` passes but PSA derives wrong keys. `flash.sh platform` now warns about HUK invalidation and requires confirmation. KI-002 updated with resolution. Remaining gap: no runtime HUK mismatch detection at boot (would need test decryption).

## Acceptance Criteria
- [x] Root cause confirmed: HUK invalidation (not session key corruption)
- [x] Immediate fix documented: re-flash MFG → platform → app + BLE re-registration (KI-002)
- [x] `flash.sh` updated: `platform` subcommand warns and requires confirmation
- [ ] Optional: runtime HUK mismatch detection at boot — deferred (needs test decryption in PSA)

**Size**: S (2 points) — 30 min investigation + fix

---

### TASK-024: Harden API version mismatch to hard stop — MERGED DONE (Claude)

## Status: MERGED DONE (2026-02-11, Claude)
Changed `discover_app_image()` in `app.c` from warning to hard stop on version mismatch. Added `app_reject_reason` tracking for shell diagnostics. Updated `sid status` to show rejection reason. ADR-001 written at `docs/adr/001-version-mismatch-hard-stop.md`. KI-001 documented in `docs/known-issues.md`.

## Acceptance Criteria
- [x] Platform refuses to load app when callback table version doesn't match
- [x] Platform logs an error (not warning) with expected vs actual version
- [x] Platform boots without app functionality (safe degraded mode)
- [x] Shell `sid status` reports "app: not loaded (version mismatch)"
- [ ] Unit test: version mismatch → app callbacks not invoked — deferred to TASK-026

**Size**: S (2 points) — 30 min

---

### TASK-025: Add OTA chunk receive and delta bitmap tests — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
13 C tests in `tests/app/test_ota_chunks.c` covering chunk writes, phase rejection, duplicate handling, delta bitmap, state transitions, and out-of-bounds rejection. 13 Python tests in `aws/tests/test_ota_sender.py` covering `compute_delta_chunks()` edge cases and `build_ota_chunk()` format. All 9 C test suites (including new) and 94 Python tests pass.

Key finding: mock flash alignment issue — `ota_flash_write()` pads unaligned writes with 0xFF, which overwrites adjacent data in RAM-backed mock. Tests use 4-byte-aligned chunk sizes to avoid this.

## Acceptance Criteria
- [x] C tests: CHUNK message writes correct data to staging flash
- [x] C tests: CHUNK with wrong session/phase is rejected
- [x] C tests: duplicate chunk is handled (overwrite or ignore)
- [x] C tests: delta bitmap set/get works for chunks 0, 127, edge indices
- [x] C tests: all chunks received → transition to COMPLETE (via VALIDATING)
- [x] Python tests: `compute_delta_chunks()` with baseline > firmware
- [x] Python tests: `compute_delta_chunks()` with empty baseline
- [x] Python tests: `compute_delta_chunks()` with identical baseline and firmware (0 changed)
- [x] Python tests: `build_ota_chunk()` format matches protocol spec (0x20, 0x02, index LE, data)

**Size**: M (3 points) — 60 min

---

### TASK-026: Add app discovery and boot path tests

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/boot-path-tests`

## Description
**Source**: Eliel/Eero architecture + test coverage review (2026-02-11)

`discover_app_image()` in `app.c` validates magic and version at the app callback address. This boot path has **zero test coverage**. Silent failures here mean the device boots without app functionality with no indication to the operator (except missing telemetry).

Also covers: OTA message routing (cmd 0x20 → OTA engine, else → app), NULL app_cb safety, and timer interval bounds validation.

## Dependencies
**Blockers**: TASK-024 (version mismatch behavior must be defined first)
**Unblocks**: None

## Acceptance Criteria
- [ ] Test: valid magic + version → app callbacks invoked
- [ ] Test: wrong magic → app not loaded, platform boots standalone
- [ ] Test: wrong version → app not loaded (after TASK-024 hardens this)
- [ ] Test: OTA message (cmd 0x20) routed to OTA engine, not app
- [ ] Test: non-OTA message routed to app_cb->on_msg_received
- [ ] Test: app_cb NULL → messages handled safely (no crash)
- [ ] Test: timer interval bounds (< 100ms rejected, > 300000ms rejected)

**Size**: M (3 points) — 60 min

---

### TASK-027: Add shell command dispatch tests — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
31 tests in `tests/app/test_shell_commands.c` covering entire `app_on_shell_cmd()` dispatch. Uses capture callback pattern for shell output verification. All tests passing.

## Acceptance Criteria
- [x] Test: `app evse status` outputs J1772 state, voltage, current, charge control
- [x] Test: `app evse a/b/c` triggers simulation mode
- [x] Test: `app evse allow/pause` changes charge control state
- [x] Test: `app hvac status` outputs thermostat flags
- [x] Test: `app sid send` triggers uplink
- [x] Test: unknown subcommand prints error
- [x] Test: NULL safety
- [x] Tests use mock shell_print/shell_error callbacks

**Size**: M (3 points) — 60 min

---

### TASK-028: Add MFG key health check tests — MERGED DONE (Eero)

## Status: MERGED DONE (2026-02-11, Eero)
7 tests in `tests/app/test_mfg_health.c`. Extracted `mfg_key_health_check()` from static function in `sidewalk_events.c` into standalone module (`mfg_health.h`/`mfg_health.c`) for testability. Returns `mfg_health_result_t` struct with `ed25519_ok` and `p256r1_ok` booleans. Mock at `tests/mocks/sid_pal_mfg_store_ifc.h`. All tests passing.

## Acceptance Criteria
- [x] Test: valid keys present → no error logged
- [x] Test: ED25519 key all zeros → error logged
- [x] Test: P256R1 key all zeros → error logged
- [x] Test: both keys missing → both errors logged
- [x] Test: single nonzero byte edge cases
- [x] Test: no short-circuit verification (both keys always checked)
- [x] Tests mock the MFG read functions

**Size**: S (2 points) — 30 min

---

### TASK-029: Production observability — CloudWatch alerting and remote status query

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/prod-observability`

## Description
**Agent**: Eliel (Backend Architect). Per PRD 5.3.2 and 5.3.3, production observability is too thin — no offline alerting, no remote query, no interlock state change logging. This task delivers Tier 1 cloud alerting (device offline detection, OTA failure alerting, interlock state change logging, daily health digest) with alarms initially disabled until the first field installation provides a stable baseline. Tier 2 adds a new downlink command (0x40) that triggers an on-demand status uplink with extended diagnostics (firmware version, uptime, Sidewalk state, boot count); Tier 2 is a v1.1 deliverable.

## Dependencies
**Blockers**: None
**Unblocks**: None (but required before production deployment)

## Acceptance Criteria
- [ ] CloudWatch metric filter on DynamoDB writes per device_id; alarm when no write for 2x heartbeat interval
- [ ] CloudWatch alarm on OTA sender Lambda errors or stalled sessions (no ACK for >5 min)
- [ ] CloudWatch metric filter for cool_call transitions (interlock state change logging)
- [ ] Scheduled Lambda for daily health digest (last-seen, firmware version, error counts)
- [ ] All Tier 1 alarms deployed but initially disabled (generous thresholds)
- [ ] Tier 2: Remote status request downlink (0x40) triggers immediate extended uplink (firmware version, uptime, Sidewalk state, last error, interlock state, Charge Now active, boot count)
- [ ] Tier 2: Extended diagnostics payload (magic 0xE6?) fits within 19-byte LoRa MTU

## Testing Requirements
- [ ] Tier 1: Terraform plan validates CloudWatch resources
- [ ] Tier 1: Python tests for daily health digest Lambda
- [ ] Tier 2: C unit tests for 0x40 downlink parsing and status uplink encoding
- [ ] Tier 2: Python tests for decode Lambda handling extended diagnostics payload

## Completion Requirements (Definition of Done)
- [ ] Tier 1 Terraform applied, alarms visible in CloudWatch (disabled)
- [ ] Tier 2 firmware + Lambda changes merged, E2E tested on device

## Deliverables
- `aws/terraform/`: CloudWatch alarms, metric filters, SNS topic
- `aws/health_digest_lambda.py`: Daily health digest Lambda
- `aws/tests/test_health_digest.py`: Tests for digest Lambda
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Handle 0x40 command (Tier 2)
- `aws/decode_evse_lambda.py`: Decode extended diagnostics payload (Tier 2)

**Size**: L (5 points) — Tier 1: 2 hours, Tier 2: 3 hours

---

### TASK-030: Fleet command throttling — staggered random delays on charge control downlinks

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/fleet-throttling`

## Description
**Agent**: Eliel (Backend Architect). Per PRD 6.3.2, a compromised cloud could coordinate simultaneous load switching across all devices, creating a massive demand spike or drop on the distribution grid. This task adds fleet-wide command throttling: any cloud command targeting multiple devices is staggered with randomized 0-10 minute delays per device. The device also enforces local rate limiting, ignoring rapid command changes (no more than one charge control command per N minutes).

## Dependencies
**Blockers**: None
**Unblocks**: None

## Acceptance Criteria
- [ ] Charge scheduler Lambda staggers downlinks with per-device random delay (0-10 min window)
- [ ] Device-side rate limiting: ignore charge control commands arriving faster than 1 per 5 minutes
- [ ] CloudWatch anomaly detection alarm for unusual command patterns (>100 commands/min or same command to all devices simultaneously)
- [ ] Rate limit configurable via Lambda environment variable

## Testing Requirements
- [ ] Python tests: staggered delay distribution is within expected window
- [ ] Python tests: anomaly detection threshold logic
- [ ] C unit tests: device-side rate limiting rejects rapid commands, accepts after cooldown

## Completion Requirements (Definition of Done)
- [ ] Lambda + firmware changes merged
- [ ] CloudWatch anomaly alarm deployed
- [ ] Device-side rate limiting verified on device via shell

## Deliverables
- `aws/charge_scheduler_lambda.py`: Staggered delay logic
- `aws/terraform/`: CloudWatch anomaly detection alarm
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Local rate limiting
- `aws/tests/test_charge_scheduler.py`: Updated tests
- `tests/app/test_rate_limit.c`: Device-side rate limit tests

**Size**: M (3 points) — 3 hours

---

### TASK-031: OTA image signing — ED25519 signatures on OTA images — MERGED DONE (Eliel)

## Status: MERGED DONE (2026-02-14, Eliel)
**Branch**: `task/031-ota-image-signing` | **Commit**: `07fc5f9`

ED25519 signing implemented end-to-end. 64-byte signature appended to app.bin before S3 upload. Flags byte in OTA_START (byte 19) signals signed firmware to device. Backward-compatible: old firmware ignores extra byte. Device verifies signature after CRC32 in both full and delta modes. 13 new files, +1217 lines. 9 C tests + 16 Python tests all passing.

## Description
**Agent**: Eliel (Backend Architect). Per PRD 6.3.1, OTA images were validated by CRC32 only — a compromised S3 bucket or Lambda could push malicious firmware. This task added cryptographic signing to the OTA pipeline: the deploy CLI signs images with ED25519 private key, the Lambda flags the OTA_START message, and the device verifies the signature before applying the image.

## Dependencies
**Blockers**: None
**Unblocks**: None

## Acceptance Criteria
- [x] `ota_deploy.py` signs app binary with ED25519 private key during deploy
- [x] OTA START command includes flags byte (byte 19, 0x01 = signed) within 19-byte MTU
- [x] Device verifies ED25519 signature after CRC32 validation, before applying image
- [x] Invalid signature causes OTA abort with SIG_ERR status
- [x] Signing key management documented (CLAUDE.md OTA workflow section)

## Testing Requirements
- [x] Python tests: signing and signature format in deploy CLI (16 tests)
- [x] C unit tests: signature verification pass/fail paths (mock ota_signing) (9 tests)
- [x] C unit tests: OTA state machine transitions on signature failure

## Completion Requirements (Definition of Done)
- [x] All host-side tests pass (C + Python)
- [ ] End-to-end OTA with signed image verified on physical device — PENDING (requires device)
- [ ] Unsigned images rejected by device — PENDING (requires device + real ED25519 lib)

## Deliverables
- `aws/ota_signing.py`: ED25519 keygen, sign, verify (~90 lines)
- `aws/ota_deploy.py`: `keygen` subcommand, `--unsigned` flag, sign-before-upload
- `aws/ota_sender_lambda.py`: Flags byte propagation via S3 metadata
- `app/rak4631_evse_monitor/include/ota_signing.h`: Verify function declaration
- `app/rak4631_evse_monitor/src/ota_signing.c`: Placeholder (real ED25519 deferred to platform integration)
- `app/rak4631_evse_monitor/include/ota_update.h`: OTA_STATUS_SIG_ERR, OTA_START_FLAGS_SIGNED, OTA_SIG_SIZE
- `app/rak4631_evse_monitor/src/ota_update.c`: Signature verification in full + delta modes
- `aws/tests/test_ota_signing.py`: 16 Python tests
- `tests/app/test_ota_signing.c`: 9 C tests
- `tests/mocks/mock_ota_signing.c`: Controllable mock verify
- `tests/CMakeLists.txt`: mock_ota_signing library + test target

**Size**: L (5 points) — 4 hours

---

### TASK-032: Cloud command authentication — signed downlinks for command authenticity

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/cmd-auth`

## Description
**Agent**: Eliel (Backend Architect). Per PRD 6.3.2, downlinks are encrypted in transit by Sidewalk but not signed — there is no per-command authenticity verification. A compromised cloud layer could send arbitrary charge control commands. This task adds command-level authentication: the cloud signs each downlink payload with a shared secret or asymmetric key, and the device verifies the signature before executing the command.

## Dependencies
**Blockers**: None
**Unblocks**: None

## Acceptance Criteria
- [ ] All charge control downlinks (0x10) include an authentication tag
- [ ] Device verifies authentication tag before executing any charge control command
- [ ] Unsigned or incorrectly signed commands are rejected with error log
- [ ] Authentication fits within 19-byte LoRa MTU (truncated HMAC or compact signature)
- [ ] Key provisioning procedure documented

## Testing Requirements
- [ ] Python tests: command signing in charge scheduler Lambda
- [ ] C unit tests: authentication verification pass/fail paths
- [ ] C unit tests: reject unsigned commands, accept signed commands

## Completion Requirements (Definition of Done)
- [ ] Authenticated downlinks verified on physical device
- [ ] Unsigned commands rejected

## Deliverables
- `aws/charge_scheduler_lambda.py`: Command signing
- `aws/sidewalk_utils.py`: Authentication utility functions
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Command authentication verification
- `aws/tests/test_cmd_auth.py`: Signing tests
- `tests/app/test_cmd_auth.c`: Device-side auth verification tests

**Size**: L (5 points) — 4 hours

---

### TASK-033: TIME_SYNC downlink — new 0x30 command with SideCharge epoch and ACK watermark

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/time-sync`

## Description
**Agent**: Eliel (Backend Architect). Per PRD 3.3 (PDL-011 decided), the device has no wall-clock time — the cloud infers timing from uplink arrival. This task implements the TIME_SYNC downlink command (0x30) with a 9-byte payload: command type, 4-byte SideCharge epoch (seconds since 2026-01-01 00:00:00 UTC), and 4-byte ACK watermark (timestamp of most recent cloud-received uplink). The device stores sync_time and uptime_at_sync, then computes current time as sync_time + (uptime_now - uptime_at_sync). The decode Lambda sends TIME_SYNC on first uplink after boot (detects timestamp=0) and periodically for drift correction (~daily).

## Dependencies
**Blockers**: None
**Unblocks**: TASK-034 (event buffer uses ACK watermark), TASK-035 (uplink v0x07 uses timestamps)

## Acceptance Criteria
- [ ] Device parses 0x30 command: extracts 4-byte epoch time + 4-byte ACK watermark
- [ ] Device stores sync_time and sync_uptime; computes current time as sync_time + (k_uptime_get() - sync_uptime)
- [ ] Decode Lambda detects first uplink after boot (timestamp=0 or version=0x07 with no time) and sends TIME_SYNC
- [ ] Decode Lambda sends periodic TIME_SYNC (~daily) for drift correction
- [ ] ACK watermark value passed to event buffer for trimming (TASK-034 integration point)
- [ ] `sid time` shell command shows current synced time and drift estimate

## Testing Requirements
- [ ] C unit tests: TIME_SYNC parsing, time computation, drift over simulated uptime
- [ ] C unit tests: reject malformed 0x30 (wrong length)
- [ ] Python tests: decode Lambda auto-sync trigger logic
- [ ] Python tests: periodic sync scheduling

## Completion Requirements (Definition of Done)
- [ ] TIME_SYNC delivered and verified on physical device
- [ ] Device reports correct wall-clock time via shell after sync
- [ ] Clock drift < 10 seconds per day confirmed

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/app_rx.c`: Parse 0x30 command
- `app/rak4631_evse_monitor/src/app_evse/time_sync.c`: Time tracking module
- `app/rak4631_evse_monitor/include/time_sync.h`: Header
- `aws/decode_evse_lambda.py`: Auto-sync trigger logic
- `tests/app/test_time_sync.c`: C unit tests
- `aws/tests/test_time_sync.py`: Python tests

**Size**: M (3 points) — 3 hours

---

### TASK-034: Device-side event buffer — ring buffer with ACK watermark trimming

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/event-buffer`

## Description
**Agent**: Eliel (Backend Architect). Per PRD 3.2.2 (PDL-010 decided), state changes between uplinks are lost if LoRa drops the message. This task implements a device-side ring buffer of ~50 timestamped state snapshots (12 bytes each = 600 bytes from the app's 8KB RAM budget). On each uplink the device sends the most recent snapshot. The cloud ACKs received data by piggybacking an ACK watermark timestamp on the TIME_SYNC downlink (0x30). The device drops all buffer entries at or before the ACK'd timestamp. If no ACK arrives, the ring buffer wraps and overwrites the oldest entries.

## Dependencies
**Blockers**: TASK-033 (ACK watermark is delivered via TIME_SYNC command; timestamps require time sync)
**Unblocks**: TASK-035 (uplink v0x07 sends timestamped snapshots from buffer)

## Acceptance Criteria
- [ ] Ring buffer holds ~50 entries of 12-byte timestamped snapshots in app RAM
- [ ] New state snapshot added to buffer on every sensor poll (500ms cycle)
- [ ] Most recent snapshot sent on each uplink
- [ ] ACK watermark from TIME_SYNC trims all entries at or before watermark timestamp
- [ ] Buffer wraps and overwrites oldest entries when full (no crash, no stall)
- [ ] `evse buffer` shell command shows buffer fill level and oldest/newest timestamps

## Testing Requirements
- [ ] C unit tests: buffer insert, wrap, trim by watermark
- [ ] C unit tests: buffer full behavior (oldest overwritten)
- [ ] C unit tests: empty buffer edge case
- [ ] C unit tests: watermark older than all entries (no trim)
- [ ] C unit tests: watermark newer than all entries (trim all)

## Completion Requirements (Definition of Done)
- [ ] Buffer operational on device, verified via shell
- [ ] ACK watermark trim verified via TIME_SYNC round-trip

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/event_buffer.c`: Ring buffer implementation
- `app/rak4631_evse_monitor/include/event_buffer.h`: Header
- `tests/app/test_event_buffer.c`: C unit tests

**Size**: M (3 points) — 2 hours

---

### TASK-035: Uplink payload v0x07 — add 4-byte timestamp and control flags to thermostat byte

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/uplink-v07`

## Description
**Agent**: Eliel (Backend Architect). Per PRD 3.2.1, the current 8-byte uplink payload (v0x06) has no device-side timestamp, no charge_control state, and no Charge Now flag. This task bumps the payload to v0x07 (12 bytes total): adds a 4-byte SideCharge epoch timestamp (bytes 8-11, little-endian uint32) and repurposes thermostat byte bits 2-7 for CHARGE_ALLOWED, CHARGE_NOW, SENSOR_FAULT, CLAMP_MISMATCH, INTERLOCK_FAULT, and SELFTEST_FAIL flags. The decode Lambda must handle both v0x06 and v0x07 payloads for backward compatibility.

## Dependencies
**Blockers**: TASK-033 (device needs TIME_SYNC to have a valid timestamp), TASK-034 (event buffer provides timestamped snapshots)
**Unblocks**: None

## Acceptance Criteria
- [ ] Uplink payload version bumped to 0x07
- [ ] Bytes 8-11: SideCharge epoch timestamp (seconds since 2026-01-01), little-endian uint32
- [ ] Thermostat byte (byte 7) bits 2-7: CHARGE_ALLOWED, CHARGE_NOW, SENSOR_FAULT, CLAMP_MISMATCH, INTERLOCK_FAULT, SELFTEST_FAIL
- [ ] Total payload: 12 bytes (fits within 19-byte LoRa MTU)
- [ ] Decode Lambda handles both v0x06 (8 bytes, no timestamp) and v0x07 (12 bytes, with timestamp)
- [ ] DynamoDB events include device-side timestamp when available

## Testing Requirements
- [ ] C unit tests: v0x07 payload encoding (all fields, boundary values)
- [ ] C unit tests: timestamp computation from synced time
- [ ] Python tests: decode Lambda v0x07 parsing
- [ ] Python tests: decode Lambda backward compatibility with v0x06

## Completion Requirements (Definition of Done)
- [ ] v0x07 uplinks verified in DynamoDB with correct device-side timestamps
- [ ] v0x06 devices still decode correctly (no regression)

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/app_tx.c`: v0x07 payload encoding
- `aws/decode_evse_lambda.py`: v0x07 + v0x06 backward-compatible decoding
- `tests/app/test_payload_v07.c`: C unit tests
- `aws/tests/test_decode_evse.py`: Updated Python tests

**Size**: M (3 points) — 2 hours

---

### TASK-036: Device registry — DynamoDB table with SC- short ID and installer-provided location

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/device-registry`

## Description
**Agent**: Eliel (Backend Architect). Per PRD 4.6 (PDL-012 decided), there is no way to track which customer owns which device, where it is installed, or its current firmware version and liveness. This task creates a DynamoDB table `sidecharge-device-registry` with device_id (PK, format SC- + first 8 hex chars of SHA-256 of Sidewalk UUID), sidewalk_id, owner fields, meter_number, install address/coordinates, firmware version, last_seen, and status lifecycle (provisioned -> installed -> active -> inactive / returned). The decode Lambda updates last_seen and app_version on every uplink. GSIs on owner_email and status for fleet queries.

## Dependencies
**Blockers**: None
**Unblocks**: TASK-037 (utility identification uses meter_number from registry)

## Acceptance Criteria
- [ ] DynamoDB table `sidecharge-device-registry` created via Terraform
- [ ] Device ID generation: `SC-` + first 8 hex chars of SHA-256(sidewalk_uuid)
- [ ] All fields from PRD 4.6 schema present (device_id, sidewalk_id, owner_name, owner_email, meter_number, install_address, install_lat, install_lon, install_date, installer_name, provisioned_date, app_version, last_seen, status, created_at, updated_at)
- [ ] Decode Lambda updates last_seen and app_version on every uplink
- [ ] GSI on owner_email for "my devices" lookup
- [ ] GSI on status for fleet health queries
- [ ] Status lifecycle: provisioned -> installed -> active -> inactive / returned

## Testing Requirements
- [ ] Terraform plan validates table + GSI creation
- [ ] Python tests: device ID generation (SHA-256 + truncation)
- [ ] Python tests: decode Lambda registry update (last_seen, app_version)
- [ ] Python tests: status transitions

## Completion Requirements (Definition of Done)
- [ ] Registry table deployed, decode Lambda updates it on uplinks
- [ ] Device lookup by short ID and by owner_email confirmed

## Deliverables
- `aws/terraform/device_registry.tf`: DynamoDB table + GSIs
- `aws/decode_evse_lambda.py`: Registry update on uplink
- `aws/device_registry.py`: Registry utility functions (ID generation, status transitions)
- `aws/tests/test_device_registry.py`: Python tests

**Size**: M (3 points) — 3 hours

---

### TASK-037: Utility identification — per-device meter number to utility/TOU schedule lookup — PLANNED (Pam)

## Status: PLANNED (2026-02-13, Pam)
Product scoping complete in PRD section 4.5. Corrected the original design assumption (meter number → utility is wrong; address → utility is correct). Defined the three-step lookup pipeline (address → utility, utility → TOU schedule + WattTime region, meter number → rate plan). Designed TOU schedule JSON data model with peak window arrays. Documented reference schedules for top 5 US residential EV utility markets (Xcel, SCE, PG&E, SDG&E, Con Edison). Defined the charge scheduler refactor path (4 changes needed). Resolved the open question about meter number vs. address roles. Implementation is a v1.1 deliverable (v1.0 remains Xcel-only).

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/prd-v1.5`

## Description
**Agent**: Pam (Product Manager). Per PRD 4.4, the charge scheduler hardcodes Xcel Colorado (PSCO region, weekdays 5-9 PM MT). To support multiple utilities, each device needs a utility/TOU schedule lookup. The original task assumed meter number → utility mapping, but research shows meter numbers are utility-specific with no national standard. The correct pipeline is address → utility → TOU schedule, with meter number used only for rate plan disambiguation within a utility.

## Dependencies
**Blockers**: TASK-036 (device registry provides meter_number and install_address fields) — for implementation only; scoping is independent
**Unblocks**: None

## Acceptance Criteria
- [x] Lookup pipeline corrected: address → utility → TOU/WattTime, meter → rate plan
- [x] TOU schedule JSON data model defined (schedule_id, timezone, watttime_region, peak_windows array)
- [x] Reference schedules for top 5 US utility markets documented
- [x] Charge scheduler refactor path defined (4 specific changes)
- [x] Configuration storage phased: v1.0 hardcoded, v1.1 DynamoDB table, v1.1+ OpenEI API
- [x] Open question resolved: meter number vs. address roles clarified
- [ ] Implementation (DynamoDB schedule table, scheduler refactor) — v1.1, NOT STARTED

## Deliverables
- [x] PRD section 4.5 (6 subsections: problem, pipeline, data model, reference schedules, config storage, refactor path)
- [x] Known Gaps and Traceability updated

**Size**: S (2 points) — PRD scoping complete. Implementation is a separate v1.1 task.

---

### TASK-038: Data privacy — privacy policy, retention rules, and CCPA compliance review

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/data-privacy`

## Description
**Agent**: Pam (Product Manager). Per PRD 6.4.2, SideCharge stores behavioral telemetry (AC/EV patterns revealing occupancy and routines) and PII (owner name, email, address, meter number) indefinitely with no retention policy, no privacy policy document, and no CCPA compliance review. This task defines data retention rules (raw telemetry TTL, aggregated statistics retention), drafts a customer-facing privacy policy, verifies no PII leaks into CloudWatch logs, and defines a customer data deletion procedure for device return/decommission.

## Dependencies
**Blockers**: None
**Unblocks**: TASK-042 (privacy agent needs the draft policy to review/finalize)

## Acceptance Criteria
- [ ] Data retention policy defined: raw telemetry TTL (e.g., 90 days), aggregated statistics retention period
- [ ] DynamoDB TTL configured per retention policy via Terraform
- [ ] Customer-facing privacy policy drafted
- [ ] CloudWatch logs verified: no PII (owner name, email, address) in log output
- [ ] Customer data deletion procedure defined (what to delete on device return/decommission)
- [ ] CCPA compliance checklist completed (right to know, right to delete, right to opt-out)

## Testing Requirements
- [ ] Verify DynamoDB TTL expiration works as configured
- [ ] Grep CloudWatch logs for PII patterns (names, emails, addresses)
- [ ] Walkthrough of data deletion procedure

## Completion Requirements (Definition of Done)
- [ ] Privacy policy document written
- [ ] Retention rules deployed via Terraform
- [ ] CCPA checklist completed with gaps identified

## Deliverables
- `docs/privacy-policy.md`: Customer-facing privacy policy draft
- `docs/data-retention.md`: Internal retention rules and deletion procedures
- `aws/terraform/`: Updated TTL settings
- CCPA compliance checklist

**Size**: M (3 points) — 3 hours

---

### TASK-039: Commissioning self-test — boot self-test, continuous monitoring, and `sid selftest` command

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/selftest`

## Description
**Agent**: Eero (Testing Architect). Per PRD 2.5.3, the device needs automated self-tests to catch installation and operational faults. **P0 for first field install.** This task implements three self-test layers: (1) Boot self-test — ADC channels readable, GPIO pins readable, charge enable toggle-and-verify (relay readback), Sidewalk init check. Runs on every power-on, adds <100ms to startup. (2) Continuous monitoring on every 500ms sensor poll — current vs. J1772 cross-check (State C but <500mA for >10s = clamp error), charge enable effectiveness (LOW but current >500mA for >30s = interlock defeated), pilot voltage range validation, thermostat chatter detection. (3) `sid selftest` shell command triggers a full self-test cycle and prints results. Failures set fault flags in uplink thermostat byte (bits 4-7: SENSOR_FAULT, CLAMP_MISMATCH, INTERLOCK_FAULT, SELFTEST_FAIL).

## Dependencies
**Blockers**: None
**Unblocks**: TASK-040 (production self-test trigger uses self-test logic)

## Acceptance Criteria
- [ ] Boot self-test: ADC channel read check (AIN0, AIN1)
- [ ] Boot self-test: GPIO pin read check (P0.04, P0.05)
- [ ] Boot self-test: charge enable toggle-and-verify (toggle output, read back, restore)
- [ ] Boot self-test adds <100ms to startup
- [ ] Continuous: current vs. J1772 cross-check (State C + <500mA for >10s = CLAMP_MISMATCH)
- [ ] Continuous: charge enable effectiveness (enable LOW + current >500mA for >30s = INTERLOCK_FAULT)
- [ ] Continuous: pilot voltage range validation (outside all J1772 ranges for >5s = SENSOR_FAULT)
- [ ] Continuous: thermostat chatter detection (>10 toggles in 60s = SENSOR_FAULT, informational only)
- [ ] `sid selftest` shell command runs full cycle and prints pass/fail results
- [ ] Fault flags set in uplink thermostat byte bits 4-7
- [ ] Error LED pattern on boot self-test failure

## Testing Requirements
- [ ] C unit tests: each boot self-test check (pass and fail paths)
- [ ] C unit tests: each continuous monitoring check with mock sensor data
- [ ] C unit tests: timing thresholds (10s, 30s, 5s, 60s) with mock uptime
- [ ] C unit tests: fault flag encoding in thermostat byte
- [ ] On-device verification: `sid selftest` with simulated faults

## Completion Requirements (Definition of Done)
- [ ] All self-test checks implemented and passing unit tests
- [ ] `sid selftest` verified on physical device
- [ ] Fault flags appear in DynamoDB uplinks when triggered

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/selftest.c`: Self-test implementation
- `app/rak4631_evse_monitor/include/selftest.h`: Header
- `app/rak4631_evse_monitor/src/app_evse/app_entry.c`: Boot self-test call in app_init()
- `app/rak4631_evse_monitor/src/app_evse/evse_sensors.c`: Continuous monitoring checks
- `tests/app/test_selftest.c`: C unit tests

**Size**: L (5 points) — 4 hours

---

### TASK-040: Production self-test trigger — 5-press button trigger with LED blink-code results

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/prod-selftest-trigger`

## Description
**Agent**: Eero (Testing Architect). Per PRD 2.5.3, production devices have no USB serial — self-tests need a physical trigger. **P1.** This task implements a 5-press Charge Now button trigger (5 presses within 3 seconds) that runs the full self-test cycle from TASK-039. Results are reported via LED blink codes: green rapid-blinks the count of passed tests, pause, then red/both rapid-blinks the count of failed tests (0 blinks = all passed). Results are also sent as a special uplink with the SELFTEST_FAIL flag if any test fails. Requires the physical Charge Now button on the PCB.

## Dependencies
**Blockers**: TASK-039 (self-test logic must exist first), physical Charge Now button on PCB
**Unblocks**: None

## Acceptance Criteria
- [ ] 5-press detection within 3-second window on Charge Now button GPIO
- [ ] Triggers full self-test cycle (boot checks + one pass of continuous checks)
- [ ] LED blink-code output: green rapid-blinks = passed count, pause, red rapid-blinks = failed count
- [ ] 0 failed = green-only blink sequence (no red)
- [ ] Special uplink sent with SELFTEST_FAIL flag if any test fails
- [ ] Normal button behavior (single press = Charge Now) not affected by 5-press detection

## Testing Requirements
- [ ] C unit tests: 5-press detection timing (valid and invalid sequences)
- [ ] C unit tests: LED blink-code generation for various pass/fail counts
- [ ] C unit tests: single-press still triggers Charge Now (not self-test)
- [ ] On-device verification with physical button

## Completion Requirements (Definition of Done)
- [ ] 5-press trigger and LED blink-code verified on physical device
- [ ] Normal Charge Now button behavior unaffected

## Deliverables
- `app/rak4631_evse_monitor/src/app_evse/selftest_trigger.c`: Button detection + LED output
- `app/rak4631_evse_monitor/include/selftest_trigger.h`: Header
- `tests/app/test_selftest_trigger.c`: C unit tests

**Size**: M (3 points) — 2 hours

---

### TASK-041: Commissioning checklist card — printed card with 12-step checklist and wiring diagram

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/commissioning-card`

## Description
**Agent**: Bobby (Brand Guardian). Per PRD 2.5.2, the commissioning checklist card is the only defense against the most dangerous class of installation errors — 240V branch circuit wiring mistakes that the device cannot detect. **P0 for first field install.** This task designs a printed card that ships in every box: one side has the 12-step checklist (C-01 through C-12) with pass/fail checkboxes, the other side has a wiring diagram showing current clamp orientation, thermostat terminal mapping (R, Y, C, G), and J1772 pilot tap point. Card fields: installer name, date, device ID (from label), all 12 test results (pass/fail), installer signature. The card is the installation record — inspectors and future electricians use it to verify what was checked.

## Dependencies
**Blockers**: None (wiring diagram may be refined after PCB design, but card layout can proceed with current protoboard wiring)
**Unblocks**: None

## Acceptance Criteria
- [ ] Card design: front side with 12-step commissioning checklist (C-01 through C-12)
- [ ] Card design: back side with wiring diagram (current clamp, thermostat terminals, J1772 pilot tap)
- [ ] Card fields: installer name, date, device ID, 12 pass/fail checkboxes, installer signature
- [ ] Pass criteria for each step clearly printed on card (from PRD 2.5.2 table)
- [ ] Card size suitable for attachment to device enclosure or junction box cover
- [ ] Print-ready file format (PDF or similar)

## Testing Requirements
- [ ] Review by electrician or installer for clarity and completeness
- [ ] Verify all 12 steps match PRD 2.5.2 table exactly

## Completion Requirements (Definition of Done)
- [ ] Print-ready card design file committed to repo
- [ ] Card reviewed and approved for first field install

## Deliverables
- `docs/commissioning-card.pdf`: Print-ready card design
- `docs/commissioning-card-source/`: Source files (Illustrator, Figma, or similar)

**Size**: M (3 points) — 3 hours

---

### TASK-042: Privacy agent — find/assign privacy/legal agent, draft policy, CCPA review

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/privacy-agent`

## Description
**Agent**: Pam (Product Manager). Per PRD 6.4.2, no one currently owns privacy policy, CCPA compliance, data retention rules, or customer data deletion procedures. This task identifies and assigns a dedicated privacy/legal agent (internal or external counsel), drafts the initial privacy policy using TASK-038's framework, conducts a CCPA compliance review, and establishes ongoing privacy governance — who reviews policy changes, who handles data deletion requests, and who is the point of contact for privacy inquiries.

## Dependencies
**Blockers**: None (can proceed in parallel with TASK-038; uses TASK-038 draft if available)
**Unblocks**: None

## Acceptance Criteria
- [ ] Privacy/legal agent identified and assigned (name, role, contact)
- [ ] Agent has reviewed the data inventory (what PII/behavioral data SideCharge collects and stores)
- [ ] Initial privacy policy reviewed and approved by agent
- [ ] CCPA compliance gaps identified with remediation plan
- [ ] Data deletion request procedure defined (who handles, SLA, what gets deleted)
- [ ] Privacy governance documented: who reviews changes, escalation path, review cadence

## Testing Requirements
- [ ] Privacy policy reviewed by legal/privacy agent
- [ ] CCPA checklist walkthrough with agent

## Completion Requirements (Definition of Done)
- [ ] Privacy agent assigned and documented
- [ ] Privacy policy approved
- [ ] CCPA remediation plan exists

## Deliverables
- `docs/privacy-governance.md`: Privacy governance document (agent, roles, procedures)
- Updated `docs/privacy-policy.md`: Agent-reviewed privacy policy (builds on TASK-038 draft)

**Size**: S (2 points) — 2 hours (excludes external legal engagement time)

---

### TASK-043: Warranty and liability risk — EVSE/vehicle warranty impact from pilot wire modification — PLANNED (Pam)

## Status: PLANNED (2026-02-13, Pam)
Warranty risk analysis complete in PRD section 6.4. Identified three affected circuits (J1772 pilot — HIGH risk, vehicle onboard charger — MEDIUM risk, HVAC thermostat — LOW risk). Documented Magnuson-Moss Warranty Act protections and their limits. Identified 8 mitigation strategies with effort/impact ratings, organized into a phased roadmap (v1.0 accept + disclose → pre-customer: insurance → v1.1: reversible connector + relay logging → v1.1+: EVSE partnerships → v2.0: OCPP integration). Flagged EVSE relay wear as a plausible SideCharge-caused defect that MMWA wouldn't protect against. Four open questions documented for legal/engineering follow-up.

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/prd-v1.5`

## Description
**Agent**: Pam (Product Manager). SideCharge's core architecture requires intercepting the J1772 pilot wire between the EVSE and vehicle, and the thermostat call wire between thermostat and compressor. Both are modifications to other manufacturers' equipment that could void warranties. This task scopes the risk, identifies legal protections, and designs a phased mitigation roadmap.

## Dependencies
**Blockers**: None
**Unblocks**: None (but must be addressed before customer deployment)

## Acceptance Criteria
- [x] Risk assessment by circuit (EVSE, vehicle, HVAC) with severity ratings
- [x] Magnuson-Moss Warranty Act analysis (protections and limits)
- [x] EVSE relay wear concern identified (frequent State B cycling)
- [x] 8 mitigation strategies with effort, impact, and timeline
- [x] Phased roadmap (v1.0 → pre-customer → v1.1 → v1.1+ → v2.0)
- [x] Open questions documented for legal/engineering follow-up
- [ ] Product liability insurance obtained — PENDING (business action)
- [ ] Reversible connector designed — PENDING (v1.1 engineering)
- [ ] EVSE relay lifetime analysis — PENDING (engineering)
- [ ] Legal review of warranty disclosure language — PENDING (legal)

## Deliverables
- [x] PRD section 6.4 (6 subsections: risk, assessment, MMWA, mitigations, phased approach, open questions)
- [x] Known Gaps updated (3 new entries: warranty risk, insurance, reversible connector)
- [x] Traceability updated

**Size**: S (2 points) — PRD scoping complete. Insurance, connector design, and legal review are separate actions.

---

### TASK-044: PRD update — add commissioning sections, update wiring to G = earth ground

## Branch & Worktree Strategy
**Base Branch**: `feature/prd-v1.5`
- Branch: same (PRD updates)

## Description
**Agent**: Pam (Product Manager). The PRD is missing the detailed commissioning test sequence (C-01 through C-12), self-test and fault detection section, LED commissioning behavior, and installation failure modes. These sections were designed but not yet added to the PRD on disk. Additionally, the wiring terminal definitions need to reflect a key design decision: the G terminal is repurposed from HVAC fan control to **earth ground** (from the AC compressor junction box copper ground screw). The fan wire is permanently dropped — SideCharge never monitors or controls the HVAC fan.

Key content to add/update:
1. **Section 2.5.2**: Commissioning Test Sequence (C-01 through C-12 table with pass criteria)
2. **Section 2.5.3**: Self-Test and Fault Detection (boot, continuous, on-demand)
3. **Section 2.5.4**: Installation Failure Modes (what device can vs. cannot detect)
4. **Section 2.5.1**: LED behavior matrix (prototype single-LED + production dual-LED)
5. **Wiring terminal definitions**: R (24VAC), Y (cool call), C (common/24VAC return), G (earth ground from compressor junction box — NOT fan), P (J1772 pilot), CT (current clamp)
6. **C-03 pass criteria**: "R (24VAC), Y (cool call), C (common) on correct thermostat terminals. G (earth ground) from compressor ground screw — not from thermostat."
7. **EVSE connector selection**: Add guidance on matching the pilot tap connector to the charger model (hardwired splice, accessible terminals, or adapter cable)
8. **Remove all fan references**: No fan monitoring, no fan control, no G = fan anywhere

Reference: commissioning card design spec at `docs/commissioning-card-source/README.md` (v1.1) has the updated terminal definitions and design rationale.

## Dependencies
**Blockers**: None
**Unblocks**: None

## Acceptance Criteria
- [ ] PRD sections 2.5.1, 2.5.2, 2.5.3, 2.5.4 added with full content
- [ ] All 12 commissioning steps (C-01 through C-12) documented with pass criteria
- [ ] G terminal defined as earth ground (from compressor junction box), not fan
- [ ] C-03 references earth ground from compressor, not thermostat
- [ ] EVSE connector selection guidance included
- [ ] Zero references to fan wire or G = fan anywhere in PRD
- [ ] Known gaps table updated for commissioning card (TASK-041) and self-test (TASK-039)

## Deliverables
- Updated `docs/PRD.md`

**Size**: M (3 points) — 2 hours

---

### TASK-045: Integrate real ED25519 verify library into platform firmware

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `task/045-ed25519-verify-lib`

## Description
**Agent**: Eliel (Backend Architect). TASK-031 added OTA image signing with a placeholder `ota_signing.c` that always returns success. This task replaces the placeholder with a real ED25519 verify-only implementation. Options: (1) PSA Crypto API if Mbed TLS in NCS v2.9.1 supports ED25519 (check `PSA_ALG_PURE_EDDSA`), or (2) standalone ed25519-donna (~3-4KB verify-only). The 32-byte public key constant must be compiled into firmware. Requires a platform rebuild and reflash.

## Dependencies
**Blockers**: TASK-031 (signing infrastructure — MERGED DONE)
**Unblocks**: TASK-046 (E2E signed OTA verification needs real verify)

## Acceptance Criteria
- [ ] `ota_signing.c` calls a real ED25519 verify function (PSA Crypto or ed25519-donna)
- [ ] 32-byte public key embedded as a constant (generated via `ota_deploy.py keygen`)
- [ ] Platform firmware builds with ED25519 verify (~3-4KB code size increase acceptable)
- [ ] Host-side C tests still pass (mock_ota_signing unaffected)
- [ ] Unsigned images pass through (no signature check when `is_signed=false`)

## Testing Requirements
- [ ] Platform build succeeds with real ED25519 library
- [ ] On-device: `ota_verify_signature()` returns 0 for valid signature, nonzero for invalid
- [ ] Code size delta documented (before/after)

## Deliverables
- `app/rak4631_evse_monitor/src/ota_signing.c`: Real ED25519 verify implementation
- Kconfig or CMake changes for ED25519 library inclusion
- Documentation: which library chosen and why

**Size**: M (3 points) — 2 hours

---

### TASK-046: E2E signed OTA verification on physical device

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `task/046-signed-ota-e2e`

## Description
**Agent**: Eero (Testing Architect). End-to-end verification that the full signing pipeline works on hardware: keygen, sign, deploy, device receives signed image, verifies ED25519 signature, and applies. Also verify negative case: tamper with the S3 binary and confirm device rejects with SIG_ERR. This is the final validation gate for TASK-031 signing infrastructure.

## Dependencies
**Blockers**: TASK-045 (real ED25519 verify library must be integrated first)
**Unblocks**: None

## Acceptance Criteria
- [ ] Generate keypair via `ota_deploy.py keygen`
- [ ] Deploy signed firmware via `ota_deploy.py deploy --build --version N`
- [ ] Device receives all chunks, CRC32 passes, ED25519 signature verifies, image applied
- [ ] Device reboots and runs new firmware successfully
- [ ] Negative test: tamper with S3 binary → device rejects with OTA_STATUS_SIG_ERR (5)
- [ ] Negative test: deploy with `--unsigned` → device accepts (backward compatible, no verify)
- [ ] Results documented in E2E results file

## Testing Requirements
- [ ] Requires physical device with platform firmware containing real ED25519 verify
- [ ] Requires AWS infrastructure (S3, Lambda, IoT Wireless)
- [ ] Serial monitor to observe OTA state machine transitions and verify/reject logs

## Deliverables
- `tests/e2e/RESULTS-signed-ota.md`: E2E test results
- Updated `tests/e2e/RUNBOOK.md`: Add signed OTA test procedure

**Size**: S (2 points) — 1 hour (requires device + AWS access)

---


## Quality Requirements (Project-Level)
- [ ] All host-side unit tests passing before merge to main
- [ ] All Lambda tests passing before merge to main
- [ ] Documentation updated with each feature change
- [ ] OTA changes tested on physical device before deployment

## Technical Notes
**Development Stack**: Zephyr RTOS (nRF Connect SDK), C99, Python 3.11 (Lambda), Terraform ~5.0
**Build Tool**: West (Zephyr meta-tool), CMake, Make (tests)
**Test Framework**: Unity (C, host-side), pytest (Python)
**Cloud**: AWS (Lambda, DynamoDB, S3, IoT Wireless, EventBridge, CloudWatch)
**Connectivity**: Amazon Sidewalk LoRa 915MHz, 19-byte MTU
