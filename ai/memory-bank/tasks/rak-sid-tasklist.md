# RAK Sidewalk EVSE Monitor — Development Task List

## Session Registry

| Agent | Role | Branch | Sessions | Tasks |
|-------|------|--------|----------|-------|
| Oliver | Architecture / OTA / infra | `main`, `feature/generic-platform` | 2026-02-11 | TASK-001, 002, 004, 006, 007, 009, 010, 014, 019 |
| Eero | Testing architect | `feature/testing-pyramid` | 2026-02-11 | TASK-003, 005, 009, 010, 011, 012, 013, 016, 018, 020, 021 |
| Pam | Product manager | `feature/privacy-scoping`, `feature/utility-identification`, `feature/warranty-scoping` | 2026-02-13 | TASK-037, 042, 043 |

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

### TASK-002: Create CLAUDE.md project configuration — DONE

## Status: DONE (2026-02-11)
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

### TASK-003: Update README.md for generic platform architecture — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-004: Add charge scheduler Lambda unit tests — DONE

## Status: DONE (2026-02-11)
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

### TASK-005: Add OTA recovery path host-side tests — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-006: Add decode Lambda unit tests — DONE

## Status: DONE (2026-02-11)
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

### TASK-007: Create E2E test plan for device-to-cloud round-trip — DONE

## Status: DONE (2026-02-11)
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

### TASK-008: Document OTA recovery and rollback procedures — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-009: Set up GitHub Actions for host-side unit tests — DONE (Oliver + Eero)

## Status: DONE (2026-02-11, Oliver initial + Eero completed)
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

### TASK-010: Set up GitHub Actions for Lambda tests + linting — DONE (Oliver + Eero)

## Status: DONE (2026-02-11, Oliver initial + Eero completed)
CI runs pytest and ruff linting. Config in `pyproject.toml` (line-length 100, E/W/F/I rules). Auto-fixed 30 violations (unused f-prefixes, unused imports). Branch: `feature/testing-pyramid`.

## Acceptance Criteria
- [x] Python tests run in CI via pytest
- [x] `aws/requirements-test.txt` exists (pytest, pytest-cov, ruff)
- [x] Python linting (ruff) added to CI
- [x] `pyproject.toml` with ruff config created
- [x] All existing code passes ruff

**Size**: S (2 points) — 30 min

---

### TASK-011: Document device provisioning workflow — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-012: Validate WattTime MOER threshold for PSCO region — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-013: OTA field reliability testing across RF conditions — PARTIAL (Eero)

## Status: PARTIAL (2026-02-11, Eero)
Test plan written at `ai/memory-bank/tasks/ota-field-test-results.md`. Defines 4 test conditions (lab, indoor 10m, near outdoor 50m, far outdoor 200m+), 3 OTA cycles each, power-cycle recovery test, success criteria, and go/no-go framework. **Execution requires physical field work with device at various locations — cannot be done remotely.**

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

### TASK-014: Create Product Requirements Document (PRD) — DONE

## Status: DONE (2026-02-11)
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

### TASK-015: Remove dead sid_demo_parser code — DONE (Claude)

## Status: DONE (2026-02-11, Claude)
Deleted 5 files (~1,600 lines) from `ext/`, removed `ext/` directory entirely. Removed source entry and include path from `CMakeLists.txt`. Grep confirmed zero references in app code. All 9 C test suites pass.

## Acceptance Criteria
- [x] Remove from `CMakeLists.txt`: `ext/sid_demo_parser.c` source entry
- [x] Delete files: all 5 ext/ files
- [x] Verify: grep for references — zero in app code
- [x] Host-side unit tests pass (9/9)

**Size**: XS (1 point) — 15 min

---

### TASK-016: Document SDK divergence and architecture decisions — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-018: Add old Grenning tests to CI — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-020: Execute E2E runbook tests on physical device — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-021: Archive or remove legacy rak1901_demo app — DONE

## Status: DONE (2026-02-11)
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
```

## Priority Order (Recommended)

| Priority | Task | Rationale |
|----------|------|-----------|
| Done | TASK-002 | CLAUDE.md — already existed |
| Done | TASK-003 | README updated (on feature/testing-pyramid) |
| Done | TASK-004 | Charge scheduler tests — 17 tests passing |
| Done | TASK-005 | OTA recovery tests — 16 tests passing |
| Done | TASK-006 | Decode Lambda tests — 20 tests passing |
| Done | TASK-007 | E2E test plan (runbook written) |
| Done | TASK-009 | CI pipeline — all 4 jobs (CMake + Grenning + cppcheck + pytest) |
| Done | TASK-010 | Python linting (ruff) added to CI |
| Done | TASK-011 | Provisioning docs at docs/provisioning.md |
| Done | TASK-012 | MOER threshold validated — keep 70% |
| Done | TASK-014 | PRD written |
| Done | TASK-016 | Architecture docs at docs/architecture.md |
| Done | TASK-018 | Grenning tests added to CI |
| Done | TASK-020 | E2E runbook executed — 6/7 pass, OTA skipped |
| Done | TASK-021 | Legacy rak1901_demo removed (commit 0a3e622) |
| Done | TASK-024 | API version mismatch hardened to hard stop (Claude) |
| Done | TASK-025 | OTA chunk + delta bitmap tests — 13 C + 13 Python (Eero) |
| Done | TASK-027 | Shell command dispatch tests — 31 tests (Eero) |
| Done | TASK-028 | MFG key health check tests — 7 tests (Eero) |
| — | TASK-019 | clang-format — DECLINED |
| Done | TASK-015 | Dead code removal — 5 files, ~1,600 lines deleted (Claude) |
| Done | TASK-023 | PSA crypto -149 root caused, flash.sh warning added (Claude + Eero) |
| Partial | TASK-013 | OTA field test plan written, execution pending (requires field work) |
| P1 | TASK-022 | BUG: Stale flash inflates OTA delta baselines (plan drafted, not approved) — KI-003 |
| P2 | TASK-001 | Merge feature branches to main |
| P2 | TASK-026 | Boot path + app discovery tests (Eero, unblocked by TASK-024) |
| Done | TASK-008 | OTA recovery runbook — 533-line runbook (Eero) |
| Done | TASK-042 | Privacy scoping — PRD section 6.4 (Pam) |
| P1 | TASK-037 | Utility identification — PRD section 4.5 (Pam) |
| P1 | TASK-043 | Warranty/liability risk — PRD section 6.5 (Pam) |

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

### TASK-023: BUG — PSA crypto AEAD error -149 after platform re-flash — DONE (Claude + Eero)

## Status: DONE (2026-02-11, Claude + Eero investigation)
Root cause confirmed: HUK regenerated on platform flash, MFG keys still present, so `mfg_key_health_check()` passes but PSA derives wrong keys. `flash.sh platform` now warns about HUK invalidation and requires confirmation. KI-002 updated with resolution. Remaining gap: no runtime HUK mismatch detection at boot (would need test decryption).

## Acceptance Criteria
- [x] Root cause confirmed: HUK invalidation (not session key corruption)
- [x] Immediate fix documented: re-flash MFG → platform → app + BLE re-registration (KI-002)
- [x] `flash.sh` updated: `platform` subcommand warns and requires confirmation
- [ ] Optional: runtime HUK mismatch detection at boot — deferred (needs test decryption in PSA)

**Size**: S (2 points) — 30 min investigation + fix

---

### TASK-024: Harden API version mismatch to hard stop — DONE (Claude)

## Status: DONE (2026-02-11, Claude)
Changed `discover_app_image()` in `app.c` from warning to hard stop on version mismatch. Added `app_reject_reason` tracking for shell diagnostics. Updated `sid status` to show rejection reason. ADR-001 written at `docs/adr/001-version-mismatch-hard-stop.md`. KI-001 documented in `docs/known-issues.md`.

## Acceptance Criteria
- [x] Platform refuses to load app when callback table version doesn't match
- [x] Platform logs an error (not warning) with expected vs actual version
- [x] Platform boots without app functionality (safe degraded mode)
- [x] Shell `sid status` reports "app: not loaded (version mismatch)"
- [ ] Unit test: version mismatch → app callbacks not invoked — deferred to TASK-026

**Size**: S (2 points) — 30 min

---

### TASK-025: Add OTA chunk receive and delta bitmap tests — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-027: Add shell command dispatch tests — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-028: Add MFG key health check tests — DONE (Eero)

## Status: DONE (2026-02-11, Eero)
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

### TASK-037: Scope utility identification and multi-utility TOU support in PRD — SCOPED (Pam)

## Status: SCOPED (2026-02-13, Pam)
## Branch: `feature/utility-identification`

PRD section 4.5 added: Utility Identification and Multi-Utility Support. Covers address→utility lookup pipeline (correcting original meter→utility assumption), TOU data model, reference schedules for top 5 US utilities, config storage phasing, and charge scheduler refactor path.

## Acceptance Criteria
- [ ] PRD documents the utility identification pipeline (address → utility → TOU schedule)
- [ ] PRD documents TOU data model (day_type, hour ranges, season)
- [ ] PRD includes reference schedules for 5 major US utilities
- [ ] PRD documents config storage phasing (env vars → DynamoDB → per-device)
- [ ] PRD documents scheduler refactor path from hardcoded Xcel to configurable

**Size**: S (2 points) — 30 min

---

### TASK-042: Privacy scoping — CCPA requirements, data retention, privacy governance — DONE (Pam)

## Status: DONE (2026-02-13, Pam)
## Branch: `feature/privacy-scoping`

PRD section 6.4 added with 7 subsections: data inventory and classification (3 tiers), data retention rules (DynamoDB 90-day TTL, CloudWatch 30-day), CCPA/CPA/VCDPA/CTDPA compliance analysis, data deletion procedures, CloudWatch PII audit, privacy governance (consultant recommendation), privacy-by-design principles. Known Gaps and Traceability tables updated.

## Acceptance Criteria
- [x] PRD documents data inventory with classification tiers
- [x] PRD documents retention rules per data store
- [x] PRD documents CCPA threshold analysis and consumer rights mapping
- [x] PRD documents data deletion procedures
- [x] PRD documents CloudWatch PII audit findings
- [x] PRD recommends privacy consultant engagement
- [x] PRD Known Gaps table updated with privacy gaps
- [x] PRD Traceability table updated

**Size**: S (2 points) — 30 min

---

### TASK-043: Warranty and liability risk assessment — SCOPED (Pam)

## Status: SCOPED (2026-02-13, Pam)
## Branch: `feature/warranty-scoping`

PRD section 6.5 added: Warranty and Liability. Documents risk that intercepting J1772 pilot wire may void EVSE or vehicle warranties. Covers per-circuit risk assessment, Magnuson-Moss Warranty Act analysis, 8 mitigations, phased compliance roadmap, and open questions.

## Acceptance Criteria
- [ ] PRD documents warranty risk per circuit (pilot, current clamp, relay, thermostat)
- [ ] PRD documents Magnuson-Moss Warranty Act applicability
- [ ] PRD documents mitigation strategies
- [ ] PRD documents phased compliance roadmap
- [ ] PRD documents open questions for legal review

**Size**: S (2 points) — 30 min

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
