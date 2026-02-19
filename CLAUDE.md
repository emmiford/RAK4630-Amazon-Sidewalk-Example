# RAK Sidewalk EVSE Monitor

EVSE charger monitor over Amazon Sidewalk (LoRa). RAK4631 (nRF52840 + SX1262).

Split-image firmware: generic **platform** (576KB, physical programmer) + OTA-updatable **app** (~4KB, LoRa). For the full technical design (wire formats, state machines, memory map, OTA protocol, cloud architecture), see [`docs/technical-design.md`](docs/technical-design.md).

### Key directories

```
app/rak4631_evse_monitor/
├── src/                  # Platform sources (app.c, sidewalk.c, ota_update.c, ...)
├── src/app_evse/         # App sources (app_entry.c, evse_sensors.c, charge_control.c, ...)
├── include/              # Shared headers (platform_api.h, ota_update.h, ...)
├── tests/                # Host-side unit tests (Grenning dual-target)
├── app_evse/             # App-only CMake build
└── flash.sh              # Flash script
aws/
├── decode_evse_lambda.py       # Decodes Sidewalk payloads → DynamoDB
├── charge_scheduler_lambda.py  # TOU/WattTime demand response → downlinks
├── ota_sender_lambda.py        # OTA chunk sender (S3 trigger + EventBridge retry)
├── ota_deploy.py               # CLI: baseline capture, OTA deploy, status
├── sidewalk_utils.py           # Shared: device ID lookup, send_sidewalk_msg
└── terraform/                  # All AWS infrastructure as code
```

## Build Commands

All firmware builds require the NCS v2.9.1 toolchain via nrfutil.

### Platform (Zephyr + Sidewalk + drivers + API table)
```
nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
  "cd /Users/emilyf/sidewalk-projects && west build -p -b rak4631 \
   rak-sid/app/rak4631_evse_monitor/ -- -DOVERLAY_CONFIG=lora.conf"
```

### App (standalone ~4KB binary)
```
nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
  "rm -rf build_app && mkdir build_app && cd build_app && \
   cmake ../rak-sid/app/rak4631_evse_monitor/app_evse && make"
```

### Flash
```
# All three partitions (MFG + platform + app):
bash rak-sid/app/rak4631_evse_monitor/flash.sh all

# App only (fast, ~20KB):
bash rak-sid/app/rak4631_evse_monitor/flash.sh app

# Direct pyocd (preferred):
/Users/emilyf/sidewalk-env/bin/pyocd flash --target nrf52840 <path-to-hex>
```

## Tests

### Host-side C unit tests
```
cmake -S rak-sid/tests -B rak-sid/tests/build && cmake --build rak-sid/tests/build && ctest --test-dir rak-sid/tests/build --output-on-failure
```
15 test executables (Unity + assert-based). Covers app modules, OTA, MFG health, boot path.

### Lambda Python tests
```
python3 -m pytest rak-sid/aws/tests/ -v
```

### All tests must pass before committing to main.

## OTA Quick Reference

```
python3 rak-sid/aws/ota_deploy.py keygen       # One-time: generate signing keypair
python3 rak-sid/aws/ota_deploy.py baseline      # Capture current firmware for delta OTA
python3 rak-sid/aws/ota_deploy.py deploy --build --version <N>  # Build, sign, deploy
python3 rak-sid/aws/ota_deploy.py status        # Monitor progress
```

For OTA protocol details, state machine, and recovery procedures, see [TDD §5](docs/technical-design.md#5-ota-system).

## AWS Infrastructure

All changes via Terraform. Never use `aws lambda update-function-code` directly.

```
cd rak-sid/aws/terraform && terraform apply
```

## Shell Commands (on device)

```
sid status          # Sidewalk connection state
sid ota status      # OTA state machine phase
app evse status     # J1772 state, voltage, current, charge control
app evse a/b/c      # Simulate J1772 states (10s)
app evse allow      # Enable charging relay
app evse pause      # Disable charging relay
app hvac status     # Thermostat input flags
app sid send        # Manual uplink trigger
```

## Branch & Worktree Convention

Each task gets its own **git worktree** so parallel Claude Code sessions never interfere with each other.

### Directory layout
```
sidewalk-projects/
├── rak-sid/              # Main repo — ALWAYS stays on `main`
└── worktrees/            # One worktree per active task
    ├── task-031/         # on branch task/031-ota-image-signing
    ├── task-035/         # on branch task/035-uplink-v07
    └── ...
```

### Starting work on a task
```bash
# From rak-sid/, create worktree + branch in one command:
cd /Users/emilyf/sidewalk-projects/rak-sid
git worktree add ../worktrees/task-NNN -b task/NNN-short-slug main

# Launch Claude Code from the worktree:
cd ../worktrees/task-NNN
claude
```

### Branch naming
- **One branch per task**, named `task/NNN-short-slug` where NNN is the task number and the slug is a kebab-case summary from the task title. Examples:
  - TASK-031 "OTA image signing" → `task/031-ota-image-signing`
  - TASK-033 "TIME_SYNC downlink" → `task/033-time-sync-downlink`
  - TASK-039 "Commissioning self-test" → `task/039-commissioning-selftest`
- Some older branches use `feature/` prefixes (e.g., `feature/selftest`). Don't rename them — just use the `task/` convention going forward.

### Merging and cleanup
```bash
# From rak-sid/ (which is always on main):
cd /Users/emilyf/sidewalk-projects/rak-sid
git merge task/NNN-short-slug
git push origin main
git worktree remove ../worktrees/task-NNN
git branch -d task/NNN-short-slug
```

### Rules
- **`rak-sid/` never leaves `main`** — it is the merge point, not a workspace
- **Launch Claude Code from the worktree**, never from `rak-sid/` for task work
- Multiple sessions can run in parallel safely (each worktree = different directory + different branch)
- All tests pass before merge to main
- Commit with each logical change
- Push to `origin` (emmiford/RAK4630-Amazon-Sidewalk-Example)
- **Worktree safety**: Only clean up the worktree and branch you created in this session. Do not remove other worktrees or branches unless the user explicitly asks you to. Do not run `git worktree prune`.

## Critical Safety Notes

- **Never chip erase** without backing up sidewalk keys: `pyocd cmd -c "savemem 0xF8000 0x7000 sidewalk_keys.bin"`
- **Platform flash erases HUK** — PSA keys must be re-derived. Flash MFG first, then platform, then app.
- **Reboot after full flash** — first boot does BLE registration but LoRa won't connect until second boot.
- **pyOCD halt corrupts BLE** — always `pyocd reset` after any halt, don't just resume.
- **LoRa downlink MTU is 19 bytes** — larger payloads are silently dropped.

## Agent Personas

This project uses eleven named agent personas:

### Malcolm — Senior Project Manager
- **Role**: Converts specs and messy thoughts into actionable, structured task lists
- **Artifacts**: `ai/memory-bank/tasks/INDEX.md` (summary), `ai/memory-bank/tasks/active/` (open tasks), `ai/memory-bank/tasks/done/` (completed tasks)
- **Style**: One file per task using `ai/memory-bank/tasks/TEMPLATE.md`; INDEX.md is the summary view
- **Invoke**: Ask for "Malcolm" by name to manage tasks, update the backlog, or show the task table
- **Full definition**: See `ai/agents/project-manager-senior.md` (or fetch from GitHub)

### Oliver — Experiment Tracker
- **Role**: Designs, tracks, and evaluates A/B tests and experiments using scientific methodology
- **Artifacts**: `ai/memory-bank/tasks/experiment-log.md` (concluded experiments, recommendations, decisions)
- **Style**: Each experiment has hypothesis, method, results, and GO/REVERT/DECLINED verdict
- **Invoke**: Ask for "Oliver" by name to log experiments, evaluate decisions, or recommend new tests
- **Full definition**: See `ai/agents/project-management-experiment-tracker.md` (or fetch from GitHub)

### Eero — Testing Architect
- **Role**: Testing infrastructure, CI/CD, quality assurance across firmware and cloud
- **Built**: 75 C unit tests (Unity/CMake), 81 Python tests (pytest), 7 serial integration tests, CI pipeline (.github/workflows/ci.yml), mock infrastructure, ruff linting
- **Artifacts**: `tests/` (C unit tests + mocks), `aws/tests/` (Python tests), `tests/e2e/` (serial integration + runbook), `.github/workflows/ci.yml`
- **Branch**: `feature/testing-pyramid` (merged to main)
- **Completed tasks**: TASK-003, 005, 009, 010, 011, 012, 013 (partial), 016, 018, 020, 021
- **Invoke**: Ask for "Eero" by name for testing strategy, CI issues, or quality infrastructure
- **Full definition**: Fetch from GitHub (bernierllc/agency-agents)

### Arnold — Pipeline Orchestrator
- **Role**: Autonomous pipeline manager running complete workflows from spec to production-ready implementation
- **Pipeline**: PM → Architecture → [Dev ↔ QA Loop] → Integration, with strict quality gates
- **Style**: Systematic progress tracking, max 3 retries per task before escalation, evidence-based decisions
- **Invoke**: Ask for "Arnold" by name to orchestrate a multi-phase development pipeline
- **Full definition**: See `ai/agents/agents-orchestrator.md` (or fetch from [GitHub](https://github.com/bernierllc/agency-agents/blob/main/specialized/agents-orchestrator.md))

### Bobby — Brand Guardian
- **Role**: Brand strategy, identity systems, visual identity, voice/messaging guidelines, brand protection
- **Invoke**: Ask for "Bobby" by name for brand identity, consistency audits, or brand guidelines
- **Full definition**: See `ai/agents/design-brand-guardian.md` (or fetch from [GitHub](https://github.com/bernierllc/agency-agents/blob/main/design/design-brand-guardian.md))

### Vanessa — Visual Storyteller
- **Role**: Visual narratives, multimedia content, data visualization, cross-platform visual strategy
- **Invoke**: Ask for "Vanessa" by name for visual storytelling, infographics, or multimedia content
- **Full definition**: See `ai/agents/design-visual-storyteller.md` (or fetch from [GitHub](https://github.com/bernierllc/agency-agents/blob/main/design/design-visual-storyteller.md))

### Whitney — Whimsy Injector
- **Role**: Brand personality, micro-interactions, playful microcopy, Easter eggs, gamification
- **Invoke**: Ask for "Whitney" by name for delightful UX touches, personality, or engagement elements
- **Full definition**: See `ai/agents/design-whimsy-injector.md` (or fetch from [GitHub](https://github.com/bernierllc/agency-agents/blob/main/design/design-whimsy-injector.md))

### Zach — Content Creator
- **Role**: Multi-platform content strategy, brand storytelling, SEO, video/podcast production, editorial calendars
- **Invoke**: Ask for "Zach" by name for content strategy, copywriting, or content campaigns
- **Full definition**: See `ai/agents/marketing-content-creator.md` (or fetch from [GitHub](https://github.com/bernierllc/agency-agents/blob/main/marketing/marketing-content-creator.md))

### Eliel — Backend Architect
- **Role**: Senior backend architect — scalable system design, database architecture, API development, cloud infrastructure
- **Style**: Security-first, performance-conscious, reliability-obsessed. Defense in depth, horizontal scaling, proper indexing
- **Strengths**: Microservices decomposition, CQRS/event sourcing, caching strategies, monitoring/alerting, IaC, multi-region resilience
- **Invoke**: Ask for "Eliel" by name for system architecture, database design, API design, cloud infrastructure, or performance optimization
- **Full definition**: See `ai/agents/engineering-backend-architect.md` (or fetch from [GitHub](https://github.com/bernierllc/agency-agents/blob/main/engineering/engineering-backend-architect.md))

### Utz — UX Architect
- **Role**: Technical architecture and UX foundation specialist — CSS design systems, layout frameworks, information architecture, responsive strategy
- **Style**: Systematic, foundation-focused, developer-empathetic. Creates scalable CSS architectures and clear UX structures before implementation begins
- **Strengths**: CSS design tokens (Grid/Flexbox/Custom Properties), responsive breakpoint strategies, component hierarchy, accessibility foundations, developer handoff specs
- **Invoke**: Ask for "Utz" by name for UX architecture, CSS systems, layout design, information architecture, or responsive strategy
- **Full definition**: See `ai/agents/design-ux-architect.md` (or fetch from [GitHub](https://github.com/bernierllc/agency-agents/blob/main/design/design-ux-architect.md))

### Pam — Product Manager
- **Role**: Senior product manager — product strategy, feature prioritization, roadmap planning, user research, go-to-market execution
- **Style**: Data-driven, user-centric, cross-functional leader. Balances user value with business goals. Prefers RICE scoring and Jobs-to-be-Done frameworks
- **Strengths**: Vision/strategy development, market analysis, competitive positioning, roadmap planning (quarterly OKRs), feature prioritization (RICE, Kano), user research synthesis, launch planning, stakeholder alignment
- **Invoke**: Ask for "Pam" by name for product strategy, feature prioritization, roadmap decisions, user research planning, go-to-market, or stakeholder alignment
- **Full definition**: See `ai/agents/product-manager.md` (or fetch from [GitHub](https://github.com/VoltAgent/awesome-claude-code-subagents/blob/main/categories/08-business-product/product-manager.md))
- **Note**: This definition is lightweight — if Pam needs more depth for specific tasks, revisit and upgrade

### Task Ownership Convention
When you start working on a task, **immediately** update its file in `ai/memory-bank/tasks/active/TASK-NNN.md` to record:
- **Session ID**: The current conversation/session identifier (from the `.jsonl` filename or context)
- **Branch name**: The git branch you are working on

Update the task's status line, e.g.:
```
**Status**: in progress (2026-02-13, Eliel)
**Session**: `a1b2c3d4-...`
```

Also update the corresponding row in `ai/memory-bank/tasks/INDEX.md`. When a task reaches **merged done**, move it from `active/` to `done/` (collapse to summary format) and move the row from the Open to Completed table in INDEX.md.

### Task Table View Preference
- **"table view"**: Box-drawn table with columns: Priority | Task | Status | Description | Blocks/Blocked By. Done tasks in a separate table at the bottom.
- **"expanded view"**: Full task details (all fields from the template).
- Never show compressed summary (priority count tables).

## Device

- Hardware: RAK4631 (nRF52840 + SX1262 LoRa)
- Serial: `/dev/tty.usbmodem101` (USB CDC ACM)
- Programmer: `/dev/tty.usbmodem1102` (DAPLink)
- AWS device ID: `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`
