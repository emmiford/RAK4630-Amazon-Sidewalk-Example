# RAK Sidewalk EVSE Monitor

EVSE charger monitor over Amazon Sidewalk (LoRa). RAK4631 (nRF52840 + SX1262).

## Architecture: Split-Image (Platform + App)

Two independent firmware images sharing flash, connected via function pointer tables:

- **Platform** (576KB @ 0x00000): Generic Sidewalk sensor device runtime. Zephyr RTOS, BLE+LoRa Sidewalk stack, OTA engine, shell, hardware drivers. Knows nothing about EVSE/J1772/charging. Requires physical programmer to update.
- **App** (4KB @ 0x90000): All EVSE domain logic — sensor interpretation, change detection, payload format, charge control, shell commands. OTA-updatable over LoRa.

The contract between them is `include/platform_api.h`:
- Platform API table at `0x8FF00` (magic `PLAT`, version 2)
- App callback table at `0x90000` (magic `SAPP`, version 3)

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

### Host-side C unit tests (app layer)
```
make -C rak-sid/app/rak4631_evse_monitor/tests/ clean test
```
32 tests. Uses Grenning dual-target pattern: same app sources compiled against mock_platform.c on the host.

### Lambda Python tests
```
python3 -m pytest rak-sid/aws/tests/ -v
```

### All tests must pass before committing to main.

## Flash Layout

| Partition | Address | Size | Content |
|-----------|---------|------|---------|
| platform | 0x00000 | 576KB | Zephyr + Sidewalk + API table @ 0x8FF00 |
| app primary | 0x90000 | 256KB | EVSE app (~4KB actual) |
| ota_meta | 0xCFF00 | 256B | OTA recovery metadata |
| ota_staging | 0xD0000 | 148KB | OTA incoming image |
| settings | 0xF5000 | 8KB | Zephyr settings storage |
| hw_unique_key | 0xF7000 | 4KB | HUK for PSA crypto |
| sidewalk | 0xF8000 | 28KB | Sidewalk session keys |
| mfg | 0xFF000 | 4KB | Device credentials |

## OTA Workflow

```
# 1. Build app with version bump
# 2. Capture baseline (current device firmware for delta comparison)
python3 rak-sid/aws/ota_deploy.py baseline

# 3. Deploy (builds, uploads to S3, triggers Lambda)
python3 rak-sid/aws/ota_deploy.py deploy --build --version <N>

# 4. Monitor
python3 rak-sid/aws/ota_deploy.py status
```

Delta mode sends only changed chunks (~2-3 chunks = seconds). Full mode sends all ~276 chunks (~69 min).

## AWS Infrastructure

All changes via Terraform (`aws/terraform/`). Never use `aws lambda update-function-code` directly.

```
cd rak-sid/aws/terraform && terraform apply
```

Components: IoT Wireless (Sidewalk), 3 Lambdas, DynamoDB (`sidewalk-v1-device_events_v2`), S3 (`evse-ota-firmware-dev`), EventBridge (scheduler + OTA retry), CloudWatch alarms.

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

## Branch Convention

- Create a feature branch for each task
- All tests pass before merge to main
- Commit with each logical change
- Push to `origin` (emmiford/RAK4630-Amazon-Sidewalk-Example)

## Critical Safety Notes

- **Never chip erase** without backing up sidewalk keys: `pyocd cmd -c "savemem 0xF8000 0x7000 sidewalk_keys.bin"`
- **Platform flash erases HUK** — PSA keys must be re-derived. Flash MFG first, then platform, then app.
- **Reboot after full flash** — first boot does BLE registration but LoRa won't connect until second boot.
- **pyOCD halt corrupts BLE** — always `pyocd reset` after any halt, don't just resume.
- **LoRa downlink MTU is 19 bytes** — larger payloads are silently dropped.

## Agent Personas

This project uses two named agent personas for project management:

### Malcolm — Senior Project Manager
- **Role**: Converts specs and messy thoughts into actionable, structured task lists
- **Artifacts**: `ai/memory-bank/tasks/rak-sid-tasklist.md` (backlog with priorities, dependencies, acceptance criteria)
- **Style**: Standardized task template with branch strategy, testing requirements, deliverables, and sizing
- **Invoke**: Ask for "Malcolm" by name to manage tasks, update the backlog, or show the task table
- **Full definition**: See `ai/agents/project-manager-senior.md` (or fetch from GitHub)

### Oliver — Experiment Tracker
- **Role**: Designs, tracks, and evaluates A/B tests and experiments using scientific methodology
- **Artifacts**: `ai/memory-bank/tasks/experiment-log.md` (concluded experiments, recommendations, decisions)
- **Style**: Each experiment has hypothesis, method, results, and GO/REVERT/DECLINED verdict
- **Invoke**: Ask for "Oliver" by name to log experiments, evaluate decisions, or recommend new tests
- **Full definition**: See `ai/agents/project-management-experiment-tracker.md` (or fetch from GitHub)

### Task Table View Preference
- **"table view"**: Box-drawn table with columns: Priority | Task | Status | Description | Blocks/Blocked By. Done tasks in a separate table at the bottom.
- **"expanded view"**: Full task details (all fields from the template).
- Never show compressed summary (priority count tables).

## Device

- Hardware: RAK4631 (nRF52840 + SX1262 LoRa)
- Serial: `/dev/tty.usbmodem101` (USB CDC ACM)
- Programmer: `/dev/tty.usbmodem1102` (DAPLink)
- AWS device ID: `b319d001-6b08-4d88-b4ca-4d2d98a6d43c`
