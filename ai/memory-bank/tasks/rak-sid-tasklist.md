# RAK Sidewalk EVSE Monitor — Development Task List

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

### TASK-003: Update README.md for generic platform architecture

## Branch & Worktree Strategy
**Base Branch**: `main` (after TASK-001 merge)

## Description
The current README covers basic build/init steps but doesn't reflect the platform/app split architecture. Update it to document the new dual-image model, app-only OTA builds, and the development workflow.

## Dependencies
**Blockers**: TASK-001
**Unblocks**: None

## Acceptance Criteria
- [ ] README documents platform vs app architecture
- [ ] Explains app-only build process (CMake standalone)
- [ ] Documents OTA deployment workflow
- [ ] Describes host-side testing with `make -C tests/`
- [ ] Includes flash memory map diagram
- [ ] Lists shell commands available (sid, evse, hvac)

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

### TASK-005: Add OTA recovery path host-side tests

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/ota-recovery-tests`

## Description
The OTA module has a boot recovery path (`ota_boot_recovery_check` → `ota_resume_apply`) that resumes interrupted flash copy operations. This safety-critical code has no test coverage. Add host-side tests using the existing Grenning dual-target mock infrastructure.

## Dependencies
**Blockers**: TASK-001 (need merged platform_api.h)
**Unblocks**: TASK-009 (recovery docs should reference tested behavior)

## Acceptance Criteria
- [ ] Tests added to `app/rak4631_evse_monitor/tests/test_app.c` or new `test_ota.c`
- [ ] Test: recovery detects APPLYING metadata at boot and resumes copy
- [ ] Test: recovery completes remaining pages (e.g., interrupted at page 3 of 5)
- [ ] Test: recovery verifies magic word after completion
- [ ] Test: recovery clears metadata after successful apply
- [ ] Test: no recovery action when metadata is clear (normal boot)
- [ ] All tests pass: `make -C app/rak4631_evse_monitor/tests/ clean test`

## Testing Requirements
- [ ] Extend mock_platform with flash simulation (staging + primary + metadata regions)
- [ ] May need mock for `ota_flash_read`, `ota_flash_write`, `ota_flash_erase_pages`

## Deliverables
- Test additions in `app/rak4631_evse_monitor/tests/`
- Possible mock extensions in `mock_platform.c/h`

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

### TASK-008: Document OTA recovery and rollback procedures

## Branch & Worktree Strategy
**Base Branch**: `main`

## Description
The OTA system has recovery metadata that survives power loss during apply, and `ota_boot_recovery_check()` resumes interrupted operations. But there is no operator-facing documentation for: what happens if OTA fails, how to diagnose, how to manually recover, what the flash states mean.

## Dependencies
**Blockers**: TASK-005 (tests validate the behavior we're documenting)
**Unblocks**: None

## Acceptance Criteria
- [ ] Document: `ai/memory-bank/tasks/ota-recovery-runbook.md`
- [ ] Documents OTA state machine: IDLE → RECEIVING → VALIDATING → APPLYING → reboot
- [ ] Documents recovery metadata: flash address, fields (state, pages_copied, total_pages)
- [ ] Documents failure modes: power loss during apply, CRC mismatch, staging corruption
- [ ] Documents manual recovery: how to read metadata via shell, how to force-clear
- [ ] Documents rollback: limitations (no automatic rollback to previous app version)
- [ ] Includes `ota_deploy.py abort` and `ota_deploy.py status` for cloud-side recovery

## Deliverables
- `ai/memory-bank/tasks/ota-recovery-runbook.md`

**Size**: S (2 points) — 30 min

---

### TASK-009: Set up GitHub Actions for host-side unit tests — PARTIAL

## Status: PARTIAL (2026-02-11)
CI exists at `.github/workflows/ci.yml` with a CMake-based C test job (`cmake -S tests -B tests/build` → ctest) covering a newer test suite at top-level `tests/`. Also includes cppcheck static analysis on `app_evse/` sources. **However**, the original 32 Grenning tests at `app/rak4631_evse_monitor/tests/` (Makefile-based) are NOT wired into CI.

## What's Done
- [x] Workflow file: `.github/workflows/ci.yml`
- [x] Triggers on: push to main + feature/*, pull requests
- [x] C unit tests via CMake/ctest (newer test suite)
- [x] cppcheck static analysis

## What's Remaining
- [ ] Add old Grenning tests (`make -C app/rak4631_evse_monitor/tests/ clean test`) to CI — see TASK-018
- [ ] README badge

**Size**: S (2 points) — 30 min

---

### TASK-010: Set up GitHub Actions for Lambda tests + linting — PARTIAL

## Status: PARTIAL (2026-02-11)
CI at `.github/workflows/ci.yml` already runs `python -m pytest aws/tests/ -v` with Python 3.11. **However**, no Python linting (ruff/flake8) is configured.

## What's Done
- [x] Python tests run in CI via pytest
- [x] `aws/requirements-test.txt` exists (pytest, pytest-cov)

## What's Remaining
- [ ] Add Python linting (ruff or flake8) to CI
- [ ] New Lambda tests (TASK-004, TASK-006) will automatically run once written

**Size**: S (2 points) — 30 min

---

### TASK-011: Document device provisioning workflow

## Branch & Worktree Strategy
**Base Branch**: `main`

## Description
The project has a `credentials.example/` directory but no documentation on how to provision a new device for Amazon Sidewalk. Document the full flow: credential generation, flashing, Sidewalk registration, and AWS IoT Wireless setup.

## Dependencies
**Blockers**: None
**Unblocks**: None

## Acceptance Criteria
- [ ] Document: `docs/provisioning.md` or section in README
- [ ] Covers: Sidewalk credential generation (Nordic tools / Sidewalk console)
- [ ] Covers: Credential file format and placement
- [ ] Covers: Flashing credentials to device
- [ ] Covers: AWS IoT Wireless device registration
- [ ] Covers: Verification steps (device connects, first uplink received)
- [ ] References `credentials.example/` as template

## Deliverables
- Provisioning documentation

**Size**: S (2 points) — 30 min

---

### TASK-012: Validate WattTime MOER threshold for PSCO region

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `feature/moer-threshold-analysis`

## Description
The charge scheduler pauses charging when WattTime MOER exceeds 70% (`MOER_THRESHOLD` env var). This threshold was set without analysis of actual PSCO (Public Service Company of Colorado) MOER distributions. It may cause excessive charging interruption (threshold too low) or minimal carbon benefit (threshold too high). Oliver's experiment log (REC-004) flagged this as high-priority.

**Reference**: `ai/memory-bank/tasks/experiment-log.md` — REC-004

## Dependencies
**Blockers**: None (can proceed independently)
**Unblocks**: None (but findings may trigger a Terraform var change)

## Acceptance Criteria
- [ ] Script: `aws/scripts/moer_analysis.py` — pulls 30 days of PSCO MOER data from WattTime API
- [ ] Analysis output: distribution histogram (what % of time is MOER above each threshold)
- [ ] Simulation: for thresholds 50%, 60%, 70%, 80%, 90% — calculate hours of charging interrupted per day
- [ ] Cross-reference with Xcel TOU peak hours (5-9 PM MT weekdays) — how much overlap?
- [ ] Recommendation: optimal threshold with rationale
- [ ] Document findings in `ai/memory-bank/tasks/moer-threshold-analysis.md`
- [ ] If threshold change warranted: update `MOER_THRESHOLD` default in `charge_scheduler_lambda.py` and Terraform variable default

## Testing Requirements
- [ ] Analysis script runs successfully with WattTime credentials
- [ ] If Lambda default changes: existing charge scheduler tests still pass (TASK-004)

## Deliverables
- `aws/scripts/moer_analysis.py`
- `ai/memory-bank/tasks/moer-threshold-analysis.md`
- Optional: Terraform var default update

**Size**: M (3 points) — 60 min

---

### TASK-013: OTA field reliability testing across RF conditions

## Branch & Worktree Strategy
**Base Branch**: `main`

## Description
OTA has only been validated in controlled lab conditions. Real-world LoRa conditions (distance, interference, weather, obstructions) have higher packet loss rates that will stress the retry/recovery mechanisms. Oliver's experiment log (REC-005) flagged this as high-priority and safety-critical — if OTA fails in the field with no physical access, the device is bricked.

**Reference**: `ai/memory-bank/tasks/experiment-log.md` — REC-005

## Dependencies
**Blockers**: TASK-001 (need merged main), TASK-005 (recovery tests validate the fallback path)
**Unblocks**: None (but results may trigger retry parameter tuning)

## Acceptance Criteria
- [ ] Test plan documented in `ai/memory-bank/tasks/ota-field-test-results.md`
- [ ] Test conditions: same-room (~1m), different-floor (~10m), outdoor (~50m), max-range (~200m+)
- [ ] Per condition: 5 OTA cycles (alternate between 2 firmware versions)
- [ ] Per cycle record: success/fail, transfer time, retry count, chunks retransmitted
- [ ] Test power-cycle-during-apply at least once (validate recovery path in field)
- [ ] Results summary: success rate per condition, mean/max transfer time, failure modes observed
- [ ] Go/no-go recommendation for production deployment

## Testing Requirements
- [ ] Two firmware versions ready for alternating deploys
- [ ] `ota_deploy.py status --watch` for monitoring each cycle
- [ ] Shell access for device-side verification (`evse status`, `sid status`)

## Deliverables
- `ai/memory-bank/tasks/ota-field-test-results.md` (plan + results)

**Size**: L (5 points) — NOTE: Multiple physical test sessions required. Plan is 30 min, execution is field work across multiple sessions.

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

### TASK-015: Remove dead sid_demo_parser code

## Branch & Worktree Strategy
**Base Branch**: `main`
- Branch: `cleanup/remove-dead-ext-code`

## Description
The `ext/` directory contains the old sid_demo_parser protocol implementation (~1,600 lines across 5 files). This was explicitly replaced by the raw 8-byte payload format in commit `550560f`. The files are still compiled via CMakeLists.txt but **no code references them**. This is confirmed dead code adding binary bloat and developer confusion.

**Source**: SDK divergence audit (2026-02-11)

## Dependencies
**Blockers**: TASK-001 (merge first, then clean up on main)
**Unblocks**: None

## Acceptance Criteria
- [ ] Remove from `CMakeLists.txt`: `ext/sid_demo_parser.c` source entry
- [ ] Delete files: `ext/sid_demo_parser.c`, `ext/sid_demo_parser.h`, `ext/sid_demo_types.h`, `ext/sid_parser_utils.h`, `ext/sid_endian.h`
- [ ] Verify: grep for `sid_demo_parser`, `sid_demo_types`, `sid_parser_utils`, `sid_endian` — zero references in app code
- [ ] Firmware builds successfully
- [ ] Host-side unit tests pass

## Deliverables
- Clean `ext/` directory (removed or empty)
- Updated `CMakeLists.txt`

**Size**: XS (1 point) — 15 min

---

### TASK-016: Document SDK divergence and architecture decisions

## Branch & Worktree Strategy
**Base Branch**: `main`

## Description
An SDK divergence audit was performed (2026-02-11) comparing the original RAK Sidewalk demo with the EVSE monitor. The findings are positive — core SDK patterns are preserved, divergences are justified — but this knowledge lives only in the audit notes. It needs to be documented so future developers understand what's custom, why, and where the boundaries are.

**Key findings from audit**:
- `sidewalk.c`: Identical to SDK demo (zero divergence)
- `sidewalk_events.c`: +140 lines of diagnostic extensions (init status tracking, MFG key health check). Justified for observability. No SDK bypass.
- `app.c`: Completely restructured for split-image architecture. Justified for OTA.
- Patches: 1 patch, 40 lines, hardware-tuning only (TCXO, SAR for RAK4631)
- SDK version: v2.9.1 (Nordic + Sidewalk)

## Dependencies
**Blockers**: TASK-001 (document post-merge architecture)
**Unblocks**: None

## Acceptance Criteria
- [ ] Document: `docs/architecture.md`
- [ ] Covers: split-image architecture (platform @ 0x0, app @ 0x90000, staging @ 0xD0000)
- [ ] Covers: platform API contract (function table, magic words, versioning)
- [ ] Covers: SDK compliance — which files are unchanged, which are extended, why
- [ ] Covers: Sidewalk initialization flow with custom diagnostics
- [ ] Covers: OTA system design (receive → validate → stage → apply → recover)
- [ ] Covers: patches applied and rationale
- [ ] Covers: what is NOT customized (Sidewalk protocol stack, BLE, radio driver)
- [ ] Includes flash memory map diagram

## Deliverables
- `docs/architecture.md`

**Size**: M (3 points) — 45 min

---

### TASK-018: Add old Grenning tests to CI

## Branch & Worktree Strategy
**Base Branch**: `main`

## Description
The original 32 Grenning dual-target tests at `app/rak4631_evse_monitor/tests/` use a Makefile build (`make clean test`). CI currently only runs the newer CMake-based test suite at top-level `tests/`. Both suites should run in CI — the Grenning tests cover the full `test_app.c` integration test that exercises on_timer change detection, heartbeat, and TX rate limiting in ways the individual CMake tests don't.

## Dependencies
**Blockers**: None
**Unblocks**: None

## Acceptance Criteria
- [ ] Add step to `.github/workflows/ci.yml` that runs `make -C app/rak4631_evse_monitor/tests/ clean test`
- [ ] Both test suites (CMake ctest + Grenning make) run and must pass
- [ ] CI fails if either suite fails

## Deliverables
- Updated `.github/workflows/ci.yml`

**Size**: XS (1 point) — 15 min

---

### TASK-019: Add clang-format configuration and CI enforcement — DECLINED

## Status: DECLINED (2026-02-11)
Evaluated and declined. Code is already consistently styled by hand. clang-format would risk reformatting existing code (brace placement, #define alignment), polluting git blame for minimal benefit. cppcheck in CI catches real bugs. Revisit if team grows or style drift becomes a problem.

**Reference**: Oliver's experiment log — EXP-009

---

### TASK-020: Execute E2E runbook tests on physical device

## Branch & Worktree Strategy
**Base Branch**: N/A (manual testing, not a code change)

## Description
The E2E runbook exists at `tests/e2e/RUNBOOK.md` but has never been formally executed with results recorded. This is hands-on work: flash the device, run through every checklist item, record pass/fail, and document any issues found. Requires physical device, Sidewalk gateway, and AWS access.

## Dependencies
**Blockers**: None (runbook exists, device should be flashable)
**Unblocks**: TASK-013 (field reliability testing builds on E2E baseline)

## Acceptance Criteria
- [ ] Every item in `tests/e2e/RUNBOOK.md` executed and checked off
- [ ] Results documented: `tests/e2e/RESULTS-<date>.md`
- [ ] Boot & connect: Sidewalk READY confirmed
- [ ] Telemetry: uplink received in DynamoDB with correct fields
- [ ] Charge control: pause/allow downlinks change device state
- [ ] OTA: deploy → status → complete → reboot cycle works
- [ ] Sensor simulation: state changes appear in cloud within 60s
- [ ] Any failures documented with root cause and follow-up task if needed

## Deliverables
- `tests/e2e/RESULTS-<date>.md`

**Size**: M (3 points) — 45 min (requires physical device access)

---

### TASK-021: Archive or remove legacy rak1901_demo app

## Branch & Worktree Strategy
**Base Branch**: `main`

## Description
`app/rak4631_rak1901_demo/` is the original sensor demo app predating the EVSE monitor. It's still in the tree but appears unmaintained. Decide whether to archive it (move to a branch/tag) or remove it to reduce confusion.

## Dependencies
**Blockers**: TASK-001 (after merge, so main is clean)
**Unblocks**: None

## Acceptance Criteria
- [ ] Decision documented: keep, archive to tag, or remove
- [ ] If removing: files deleted, any references in CMakeLists/west.yml cleaned up
- [ ] If archiving: tagged as `archive/rak1901-demo` before deletion from main
- [ ] Build still succeeds after cleanup

## Deliverables
- Clean tree (or documented decision to keep)

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
```

## Priority Order (Recommended)

| Priority | Task | Rationale |
|----------|------|-----------|
| Done | TASK-004 | Charge scheduler tests — 17 tests passing |
| Done | TASK-006 | Decode Lambda tests — 20 tests passing |
| Done | TASK-007 | E2E test plan (runbook written) |
| Done | TASK-009 | CI pipeline (partial — CMake tests + cppcheck + pytest) |
| Done | TASK-010 | Lambda tests in CI (partial — pytest runs, no linting) |
| —  | TASK-019 | clang-format — DECLINED |
| P1 | TASK-005 | Safety-critical OTA recovery path |
| P1 | TASK-012 | Unvalidated MOER threshold in production (Oliver REC-004) |
| P1 | TASK-013 | OTA field reliability — safety-critical (Oliver REC-005) |
| P1 | TASK-015 | Dead code removal — quick win, reduces confusion |
| P1 | TASK-020 | Execute E2E runbook — validate the system works end-to-end |
| P2 | TASK-003 | README update |
| P2 | TASK-016 | Architecture documentation (captures SDK audit findings) |
| P2 | TASK-018 | Add Grenning tests to CI (gap in current CI) |
| P3 | TASK-008 | OTA recovery runbook (blocked by TASK-005) |
| P3 | TASK-011 | Provisioning docs |
| P4 | TASK-021 | Legacy cleanup |

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
