# Architecture: Split-Image Firmware

## Overview

The RAK4631 EVSE monitor uses a split-image firmware architecture: a generic **platform** and an OTA-updatable **app**. The two images share flash but are independently compiled and linked. They communicate exclusively through versioned function pointer tables at fixed addresses.

This separation exists so that the app (EVSE domain logic) can be updated over-the-air via Sidewalk LoRa without reflashing the platform (Zephyr RTOS + Sidewalk stack + OTA engine), which requires a physical programmer.

## Memory Layout

```
0x00000 ┌────────────────────────────┐
        │  Platform (576KB)          │
        │  Zephyr RTOS + Sidewalk    │
        │  OTA engine, shell,        │
        │  hardware drivers          │
        │  No EVSE knowledge         │
0x8FF00 │  Platform API table (256B) │ ← struct platform_api
0x90000 ├────────────────────────────┤
        │  App primary (256KB)       │
        │  App callback table        │ ← struct app_callbacks
        │  EVSE domain logic (~4KB)  │
        │  (remainder unused)        │
0xCFF00 ├────────────────────────────┤
        │  OTA metadata (256B)       │ ← recovery state
0xD0000 ├────────────────────────────┤
        │  OTA staging (148KB)       │ ← incoming firmware
0xF5000 ├────────────────────────────┤
        │  Zephyr settings (8KB)     │
0xF7000 ├────────────────────────────┤
        │  HW unique key (4KB)       │ ← PSA crypto
0xF8000 ├────────────────────────────┤
        │  Sidewalk keys (28KB)      │ ← session keys
0xFF000 ├────────────────────────────┤
        │  MFG credentials (4KB)     │ ← device identity
0x100000└────────────────────────────┘
```

RAM is split similarly: platform uses most of nRF52840's 256KB SRAM; the app gets the last 8KB (0x2003E000–0x20040000).

## API Contract: platform_api.h

Both tables carry a magic word and version number so each side can detect incompatible images at boot.

### Platform API (at 0x8FF00, magic "PLAT", version 3)

The platform provides 22 function pointers that the app calls:

| Category | Functions | Purpose |
|----------|-----------|---------|
| Sidewalk | `send_msg`, `is_ready`, `get/set_link_mask`, `factory_reset` | Messaging and link control |
| Hardware | `adc_read_mv`, `gpio_get`, `gpio_set`, `led_set` | Sensor reading and relay control |
| System | `uptime_ms`, `reboot` | Timekeeping and recovery |
| Timer | `set_timer_interval` | Configure on_timer period |
| Logging | `log_inf`, `log_err`, `log_wrn` | Printf-style logging to Zephyr backend |
| Shell | `shell_print`, `shell_error` | Output within shell command handlers |
| Memory | `malloc`, `free` | Dynamic allocation via Sidewalk HAL |
| MFG | `mfg_get_version`, `mfg_get_dev_id` | Device identity |

### App Callbacks (at 0x90000, magic "SAPP", version 3)

The app provides 7 callback pointers that the platform invokes:

| Callback | When called |
|----------|-------------|
| `init(api)` | Once at boot; app receives the platform API pointer |
| `on_ready(bool)` | Sidewalk connection state changes |
| `on_msg_received(data, len)` | Downlink message arrives (non-OTA) |
| `on_msg_sent(msg_id)` | Uplink acknowledged |
| `on_send_error(msg_id, err)` | Uplink failed |
| `on_timer()` | Periodic timer fires (app configures interval) |
| `on_shell_cmd(cmd, args, print, error)` | User types `app <cmd>` in shell |

### Versioning Rules

- Magic mismatch: platform refuses to load app (boots in platform-only mode)
- Version mismatch: platform refuses to load app (mismatched function pointer tables cause hard faults)
- Both sides must have exactly matching version numbers — no forward/backward compatibility
- `sid status` shell command reports the rejection reason ("bad magic" or "version mismatch")

## Boot Sequence

```
1. Platform boots (Zephyr main → app_start())
2. ota_init() — initialize OTA module
3. ota_boot_recovery_check() — resume interrupted OTA apply if needed
4. discover_app_image() — read app callback table at 0x90000
   ├─ Validate magic (0x53415050 = "SAPP")
   └─ Validate version
5. app_cb->init(&platform_api_table) — app bootstraps
6. Start Sidewalk: sid_platform_init() → sid_init() → sid_start()
7. Start periodic timer (500ms initial, then app-configured interval)
8. Event loop:
   ├─ Sidewalk msg → check cmd type 0x20 (OTA) → else app_cb->on_msg_received()
   ├─ Timer tick → app_cb->on_timer()
   └─ Shell "app ..." → app_cb->on_shell_cmd()
```

## OTA Update Flow

```
IDLE → [START msg] → RECEIVING → [all chunks] → VALIDATING → [CRC OK]
  → COMPLETE → [15s delay for uplink] → APPLYING → reboot

Recovery: if power lost during APPLYING, ota_boot_recovery_check()
detects metadata state and resumes page-by-page copy.
```

Key design decisions:
- **15-second apply delay**: lets the COMPLETE uplink transmit before reboot
- **Page-by-page copy with progress tracking**: metadata at 0xCFF00 records pages_copied, surviving power loss
- **Magic verification after apply**: reads APP_CALLBACK_MAGIC from primary after copy to detect corruption
- **Delta mode**: only changed chunks sent; merges baseline (primary) with received chunks (staging)

## SDK Compliance

The project is built on Nordic nRF Connect SDK v2.9.1 with the Sidewalk add-on.

| Component | File | SDK Standard? | Custom Extensions |
|-----------|------|---------------|-------------------|
| Sidewalk thread | `sidewalk.c` (79 lines) | 100% | None |
| Sidewalk events | `sidewalk_events.c` (472 lines) | ~90% | MFG key health check, init state tracking |
| Platform boot | `app.c` (623 lines) | ~30% | Split-image discovery, OTA intercept, shell dispatch |
| Platform API impl | `platform_api_impl.c` (449 lines) | ~30% | Hardware abstraction layer |
| API contract | `platform_api.h` (117 lines) | Custom | Entire versioned ABI |
| App entry | `app_entry.c` (284 lines) | Custom | EVSE domain logic |

### What's unchanged from SDK

- `sidewalk.c`: Thread-based event dispatch, identical to Nordic demo
- Sidewalk protocol stack: BLE, FSK, LoRa radio drivers
- Zephyr RTOS kernel, device tree, flash drivers

### What's extended

- `sidewalk_events.c`: Added `mfg_key_health_check()` (detects lost ED25519/P256R1 keys after reflash) and `sid_init_status_t` tracking for diagnostics. All base event handlers preserved.

### What's custom

- Split-image architecture and API contract
- OTA engine (receive, validate, stage, apply, recover)
- Hardware abstraction (ADC channels, GPIO pin mapping, shell wrappers)
- All app-layer EVSE logic

### Patches

One patch applied via `west patch`: `0001-sidewalk-rak4631-tcxo-settings.patch` (40 lines).

Changes the SX1262 radio configuration for RAK4631 hardware:
- Enables TCXO control via DIO3 pin (RAK4631 has an external TCXO that needs explicit enable)
- Sets 1.8V control voltage and 2ms startup timeout
- Without this patch, LoRa TX frequency stability is poor

The patch is board-conditional (`#if defined(CONFIG_BOARD_RAK4631)`) so it doesn't affect other boards.

## GPIO Pin Mapping

| Abstract Pin | Physical Function | Direction | Usage |
|-------------|-------------------|-----------|-------|
| 0 | charge_enable | Output | Relay control (HIGH=allow, LOW=pause) |
| 1 | heat_call | Input | Thermostat heat demand |
| 2 | cool_call | Input | Thermostat cool demand |

ADC channel 0 reads the J1772 pilot signal voltage (0–3300mV). ADC channel 1 reads the current clamp (0–3300mV → 0–30A).

## Testing Architecture

The `platform_api` function pointer table is a natural mock seam. All app modules access hardware exclusively through `api->*` calls, making host-side testing straightforward:

- **Unity/CMake tests** (`tests/`): 75 tests across 6 test suites (sensors, charge control, thermostat, TX, RX, OTA recovery). Mock platform API + mock Zephyr flash for OTA.
- **Grenning tests** (`app/rak4631_evse_monitor/tests/`): 32 tests using the dual-target pattern. Same app sources compiled against `mock_platform.c`.
- **Python tests** (`aws/tests/`): 81 tests covering Lambda decode, charge scheduler, OTA sender, and integration chains.
