# RAK Sidewalk EVSE Monitor

EV charger monitoring over Amazon Sidewalk using RAK4631 (nRF52840 + SX1262 LoRa).

Monitors J1772 pilot state, charging current, and thermostat inputs. Sends status uplinks over Sidewalk LoRa. Receives charge-control downlinks for demand response (TOU peak pricing + WattTime grid carbon signal).

## Architecture

Split-image firmware: a generic **platform** and an OTA-updatable **app**.

```
┌─────────────────────────────────────────┐
│  AWS Cloud                              │
│  decode Lambda → DynamoDB               │
│  charge scheduler Lambda → downlinks    │
│  OTA sender Lambda → firmware chunks    │
└──────────────┬──────────────────────────┘
               │ Amazon Sidewalk (LoRa 915MHz)
┌──────────────┴──────────────────────────┐
│  Platform (576KB @ 0x00000)             │
│  Zephyr RTOS, BLE+LoRa Sidewalk stack, │
│  OTA engine, shell, hardware drivers    │
│  Generic — no EVSE knowledge            │
├─────────────────────────────────────────┤
│  App (4KB @ 0x90000)  ← OTA updatable  │
│  J1772 sensing, charge control,         │
│  thermostat inputs, payload format,     │
│  change detection, shell commands       │
└─────────────────────────────────────────┘
```

The two images communicate via function pointer tables defined in [`include/platform_api.h`](app/rak4631_evse_monitor/include/platform_api.h):
- **Platform API** at `0x8FF00` — services the app can call (ADC, GPIO, Sidewalk send, timers, logging)
- **App callbacks** at `0x90000` — hooks the platform calls into (init, on_timer, on_msg_received, shell commands)

## Prerequisites

- [nRF Connect SDK v2.9.1](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html) via nrfutil toolchain-manager
- [pyOCD](https://pyocd.io/) for flashing
- Python 3.9+ for AWS tooling and tests

## Getting Started

### Workspace initialization
```shell
west init -m https://github.com/emmiford/RAK4630-Amazon-Sidewalk-Example.git rak-sid-workspace
cd rak-sid-workspace
west update
cd rak-sid
west patch -a
```

### Build platform
```shell
nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
  "cd .. && west build -p -b rak4631 rak-sid/app/rak4631_evse_monitor/ \
   -- -DOVERLAY_CONFIG=lora.conf"
```

### Build app (standalone ~4KB)
```shell
nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
  "rm -rf build_app && mkdir build_app && cd build_app && \
   cmake ../rak-sid/app/rak4631_evse_monitor/app_evse && make"
```

### Flash
```shell
# All partitions (MFG credentials + platform + app):
bash app/rak4631_evse_monitor/flash.sh all

# App only (fast, ~20KB):
bash app/rak4631_evse_monitor/flash.sh app
```

After a full flash, reboot the device once for LoRa to connect (first boot does BLE registration only).

### Docker build (alternative)
```shell
./docker/build-docker-image
./docker/dock-run west build -p -b rak4631 app/rak4631_evse_monitor/ \
  -- -DOVERLAY_CONFIG="lora.conf"
```

## Testing

CI runs automatically on push/PR via GitHub Actions: static analysis (cppcheck), C unit tests, and Python tests.

### Host-side C unit tests

**Unity/CMake suite** (59 tests — sensors, charge control, thermostat, TX/RX):
```shell
cmake -S tests -B tests/build && cmake --build tests/build && \
  ctest --test-dir tests/build --output-on-failure
```

**Grenning suite** (32 tests — integration: change detection, heartbeat, rate limiting):
```shell
make -C app/rak4631_evse_monitor/tests/ clean test
```

Both suites compile app sources against a mock `platform_api` on the host — no hardware needed.

### Python tests (81 tests — decode, scheduler, OTA deploy, Lambda chain)
```shell
pip install -r aws/requirements-test.txt
python3 -m pytest aws/tests/ -v
```

### Integration tests (7 tests — requires device connected via USB)
```shell
pytest tests/integration/ -v --serial-port /dev/cu.usbmodem101
```

## Flash Memory Map

| Partition | Address | Size | Content |
|-----------|---------|------|---------|
| platform | `0x00000` | 576KB | Zephyr + Sidewalk + API table |
| app primary | `0x90000` | 256KB | EVSE app (~4KB actual) |
| ota_meta | `0xCFF00` | 256B | OTA recovery metadata |
| ota_staging | `0xD0000` | 148KB | OTA incoming image |
| settings | `0xF5000` | 8KB | Zephyr settings |
| sidewalk | `0xF8000` | 28KB | Sidewalk session keys |
| mfg | `0xFF000` | 4KB | Device credentials |

## OTA Updates

The app partition can be updated over-the-air via Sidewalk LoRa:

```shell
# Capture baseline of current device firmware
python3 aws/ota_deploy.py baseline

# Build, upload to S3, trigger OTA
python3 aws/ota_deploy.py deploy --build --version <N>

# Monitor progress
python3 aws/ota_deploy.py status
```

Delta mode compares against a stored baseline and sends only changed chunks (typically 2-3 chunks, completes in seconds). Full mode sends all ~276 chunks (~69 minutes at LoRa rates).

## Shell Commands

Connect via serial (`/dev/tty.usbmodem101`, 115200 baud):

| Command | Description |
|---------|-------------|
| `sid status` | Sidewalk connection state |
| `sid ota status` | OTA state machine phase |
| `app evse status` | J1772 state, pilot voltage, current, charge control |
| `app evse a/b/c` | Simulate J1772 states A/B/C for 10s |
| `app evse allow` | Enable charging relay |
| `app evse pause` | Disable charging relay |
| `app hvac status` | Thermostat input flags (heat/cool calls) |
| `app sid send` | Manual uplink trigger |

## AWS Infrastructure

All cloud resources managed via Terraform:

```shell
cd aws/terraform && terraform apply
```

- **decode Lambda**: Parses Sidewalk uplinks → DynamoDB
- **charge scheduler Lambda**: TOU + WattTime demand response → charge control downlinks (EventBridge, every 5 min)
- **OTA sender Lambda**: S3-triggered firmware chunk delivery with EventBridge retry
- **DynamoDB**: Device events and state (`sidewalk-v1-device_events_v2`)
- **S3**: Firmware binaries (`evse-ota-firmware-dev`)

## Project Structure

```
app/rak4631_evse_monitor/
├── src/                  # Platform sources
├── src/app_evse/         # App sources (EVSE domain logic)
├── include/              # Shared headers (platform_api.h, ota_update.h)
├── app_evse/             # App-only CMake build
├── tests/                # Host-side unit tests
└── flash.sh              # Flash helper script
aws/
├── terraform/            # Infrastructure as code
├── tests/                # Lambda test suite
├── decode_evse_lambda.py
├── charge_scheduler_lambda.py
├── ota_sender_lambda.py
├── ota_deploy.py         # OTA deployment CLI
└── sidewalk_utils.py     # Shared utilities
```
