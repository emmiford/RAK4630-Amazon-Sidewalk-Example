# SideCharge Technical Design Document

**Status**: Living document — updated as the implementation evolves
**Last updated**: 2026-02-16

This is the single authoritative reference for how the SideCharge firmware and cloud
systems work at the wire format, state machine, and protocol level. For *why* decisions
were made, see the ADRs in `docs/adr/`. For operational procedures, see
`docs/provisioning.md`. For product requirements, see `docs/PRD.md`.

---

## 1. System Architecture

### 1.1 Split-Image Design

The RAK4631 EVSE monitor uses two independently compiled firmware images sharing flash:

| Image | Size | Address | Update Method | Content |
|-------|------|---------|---------------|---------|
| **Platform** | 576KB | 0x00000 | Physical programmer | Zephyr RTOS, BLE+LoRa Sidewalk stack, OTA engine, shell, hardware drivers. Knows nothing about EVSE/J1772/charging. |
| **App** | ~4KB actual (256KB partition) | 0x90000 | OTA over LoRa | All EVSE domain logic: sensor interpretation, change detection, payload format, charge control, shell commands. |

The separation exists so the app can be updated over-the-air via Sidewalk LoRa (~40 seconds
for a delta update) without reflashing the platform (which requires a physical programmer).

### 1.2 Memory Map

```
0x00000 ┌────────────────────────────┐
        │  Platform (576KB)          │
        │  Zephyr RTOS + Sidewalk    │
        │  OTA engine, shell,        │
        │  hardware drivers          │
        │  No EVSE knowledge         │
0x8FF00 │  Platform API table (256B) │ ← struct platform_api
0x90000 ├────────────────────────────┤
        │  App callback table        │ ← struct app_callbacks
        │  App image (~4KB actual)   │
        │  EVSE domain logic         │
        │  (remainder unused)        │
0xCFF00 ├────────────────────────────┤
        │  OTA metadata (256B)       │ ← recovery state
0xD0000 ├────────────────────────────┤
        │  OTA staging (148KB)       │ ← incoming firmware
0xF5000 ├────────────────────────────┤
        │  Zephyr settings (8KB)     │ ← NVS
0xF7000 ├────────────────────────────┤
        │  HW unique key (4KB)       │ ← PSA crypto
0xF8000 ├────────────────────────────┤
        │  Sidewalk keys (28KB)      │ ← session keys
0xFF000 ├────────────────────────────┤
        │  MFG credentials (4KB)     │ ← device identity
0x100000└────────────────────────────┘
```

RAM: platform uses most of the nRF52840's 256KB SRAM; the app gets the last 8KB
(0x2003E000–0x20040000). The event buffer (600B) is the app's largest single allocation.

### 1.3 Boot Sequence

```
1. Platform boots (Zephyr main → app_start())
2. ota_init() — initialize OTA module
3. ota_boot_recovery_check() — resume interrupted OTA apply if needed
4. discover_app_image() — read app callback table at 0x90000
   ├─ Validate magic (must equal 0x53415050 = "SAPP")
   └─ Validate version (must equal APP_CALLBACK_VERSION exactly)
5. app_cb->init(&platform_api_table) — app bootstraps all modules
6. Start Sidewalk: sid_platform_init() → sid_init() → sid_start()
7. Start periodic timer (app requests 500ms via set_timer_interval())
8. Event loop:
   ├─ Sidewalk msg → check cmd type 0x20 (OTA) → else app_cb->on_msg_received()
   ├─ Timer tick → app_cb->on_timer()
   └─ Shell "app ..." → app_cb->on_shell_cmd()
```

If magic or version check fails, the device boots in **platform-only mode**: Sidewalk
connectivity and OTA engine still work (so a corrected app can be pushed OTA), but no
app callbacks fire and no EVSE telemetry is sent.

---

## 2. Platform API Reference

The contract between platform and app is defined entirely in `include/platform_api.h` (119 lines).

### 2.1 Platform API Table

**Address**: `0x8FF00` (last 256 bytes of the 576KB platform partition)
**Magic**: `0x504C4154` ("PLAT")
**Version**: 3

The platform provides 20 function pointers that the app calls:

```c
struct platform_api {
    uint32_t magic;
    uint32_t version;

    /* Sidewalk (5) */
    int   (*send_msg)(const uint8_t *data, size_t len);
    bool  (*is_ready)(void);
    int   (*get_link_mask)(void);
    int   (*set_link_mask)(uint32_t mask);
    int   (*factory_reset)(void);

    /* Hardware (4) */
    int   (*adc_read_mv)(int channel);          /* returns millivolts, <0 on error */
    int   (*gpio_get)(int pin_index);           /* returns 0/1, <0 on error */
    int   (*gpio_set)(int pin_index, int val);  /* returns 0 on success */
    void  (*led_set)(int led_id, bool on);      /* control board LEDs (0-3) */

    /* System (2) */
    uint32_t (*uptime_ms)(void);
    void     (*reboot)(void);

    /* Timer (1) */
    int   (*set_timer_interval)(uint32_t interval_ms);

    /* Logging (3) */
    void (*log_inf)(const char *fmt, ...);
    void (*log_err)(const char *fmt, ...);
    void (*log_wrn)(const char *fmt, ...);

    /* Shell output (2) */
    void (*shell_print)(const char *fmt, ...);
    void (*shell_error)(const char *fmt, ...);

    /* Memory (2) */
    void *(*malloc)(size_t size);
    void  (*free)(void *ptr);

    /* MFG diagnostics (2) */
    uint32_t (*mfg_get_version)(void);
    bool     (*mfg_get_dev_id)(uint8_t *id_out);
};
```

### 2.2 App Callback Table

**Address**: `0x90000` (start of app partition)
**Magic**: `0x53415050` ("SAPP")
**Version**: 3

The app provides 7 callbacks that the platform invokes:

```c
struct app_callbacks {
    uint32_t magic;
    uint32_t version;

    int   (*init)(const struct platform_api *api);  /* once at boot */
    void  (*on_ready)(bool ready);                  /* Sidewalk connection state */
    void  (*on_msg_received)(const uint8_t *data, size_t len); /* downlink (non-OTA) */
    void  (*on_msg_sent)(uint32_t msg_id);          /* uplink acknowledged */
    void  (*on_send_error)(uint32_t msg_id, int error);
    void  (*on_timer)(void);                        /* periodic, app-configured interval */
    int   (*on_shell_cmd)(const char *cmd, const char *args,
                          void (*print)(const char *fmt, ...),
                          void (*error)(const char *fmt, ...));
};
```

### 2.3 Version Compatibility

Per ADR-001, version mismatch is a **hard stop**:

- Magic mismatch: platform refuses to load app (boots in platform-only mode)
- Version mismatch: platform refuses to load app (mismatched function pointer layouts
  cause hard faults on bare metal — there is no safe way to proceed)
- Both sides must have **exactly matching** version numbers. No forward/backward
  compatibility.
- API version is bumped **only** when the function pointer table layout changes. App-side
  changes (new payload format, new sensor logic) never require a version bump.

---

## 3. Uplink Protocol

### 3.1 EVSE Payload v0x08 (Current)

10 bytes. Fits within the 19-byte LoRa uplink MTU with 9 bytes spare.

```
Offset  Size  Field               Type          Description
------  ----  -----               ----          -----------
0       1     Magic               uint8         0xE5 (constant)
1       1     Version             uint8         0x08
2       1     J1772 state         uint8         Enum 0-6 (see §6.1)
3-4     2     Current draw        uint16_le     Milliamps (0-30000)
5       1     Flags               uint8         Bitfield (see §3.2)
6-9     4     Timestamp           uint32_le     SideCharge epoch (see §7.1)
                                                0 = not yet synced
```

AC supply voltage is assumed to be 240V for all power calculations. The device does not
measure line voltage. The J1772 pilot signal voltage (ADC AIN0) is used internally for
state classification (A-F) but is not included in the uplink — the J1772 state enum
(byte 2) is sufficient.

Encoding example:
```
E5 08 03 D0 07 06 39 A2 04 00
│  │  │  └─────┘ │  └──────────┘
│  │  │  2000 mA │  SideCharge epoch 304697 (~3.5 days)
│  │  State C (charging)  Flags: COOL | CHARGE_ALLOWED
│  Version 0x08
Magic 0xE5
```

### 3.2 Flags Byte (Byte 5) Bit Map

```
Bit  Mask  Name              Source
---  ----  ----              ------
0    0x01  (reserved)        reserved for future heat pump support (always 0)
1    0x02  COOL              thermostat cool demand GPIO
2    0x04  CHARGE_ALLOWED    charge control relay state
3    0x08  CHARGE_NOW        reserved (TASK-040, always 0)
4    0x10  FAULT_SENSOR      ADC/GPIO read failure or pilot out-of-range
5    0x20  FAULT_CLAMP       current vs. J1772 state disagreement
6    0x40  FAULT_INTERLOCK   charge enable ineffective / relay stuck
7    0x80  FAULT_SELFTEST    boot self-test failure (latched until reset)
```

Bit 0 is reserved for future heat pump support (always 0 in v1.0). Bit 1 comes from
`thermostat_flags_get()`. Bit 2 is OR'd in by `app_tx.c` from
`charge_control_is_allowed()`. Bits 4-7 come from `selftest_get_fault_flags()`.

### 3.3 Legacy Formats

The decode Lambda handles three payload formats, identified by byte 0 and byte 1:

| Format | Magic | Version | Size | Differences from v0x08 |
|--------|-------|---------|------|------------------------|
| **v0x08** (current) | 0xE5 | 0x08 | 10B | Full format. Pilot voltage removed; HEAT flag reserved. |
| **v0x07** | 0xE5 | 0x07 | 12B | Includes pilot voltage (bytes 3-4); HEAT flag active. |
| **v0x06** | 0xE5 | 0x06 | 8B | No timestamp (bytes 8-11). Flags byte has thermostat bits only. |
| **sid_demo legacy** | varies | — | 7B+ | Wrapped in demo protocol headers. Inner payload: type(1)+j1772(1)+voltage(2)+current(2)+therm(1). Offset-scanned. |

Backward-compatible: version byte (byte 1) dispatches to the correct decoder. Old
devices sending v0x07 or v0x06 continue to be decoded correctly.

### 3.4 Rate Limiting and Heartbeat

| Parameter | Value | Source |
|-----------|-------|--------|
| Minimum uplink interval | 5 seconds | `MIN_SEND_INTERVAL_MS` in `app_tx.c` |
| Heartbeat interval | 60 seconds | `HEARTBEAT_INTERVAL_MS` in `app_entry.c` |
| Poll interval | 500 ms | `POLL_INTERVAL_MS` in `app_entry.c` |
| Change detection threshold | J1772 state, current on/off (>500mA), thermostat flags | `app_on_timer()` in `app_entry.c` |

The app sends an uplink on **any state change** or on **heartbeat expiry**, whichever
comes first. Rate limiting prevents flooding during rapid state transitions (e.g.,
vehicle plug wiggle).

### 3.5 OTA Uplinks

OTA uplinks use command type 0x20 with device→cloud subtypes:

**ACK (0x80) — 7 bytes:**
```
Byte 0:   0x20  (cmd type)
Byte 1:   0x80  (subtype)
Byte 2:   status (0=OK, 1-5=error codes)
Byte 3-4: next_chunk (uint16_le, next expected chunk index)
Byte 5-6: chunks_received (uint16_le, total chunks received so far)
```

**COMPLETE (0x81) — 7 bytes:**
```
Byte 0:   0x20
Byte 1:   0x81
Byte 2:   result (0=OK, 1=CRC_ERR, 5=SIG_ERR, etc.)
Byte 3-6: crc32_calc (uint32_le, CRC32 computed over staged image)
```

**STATUS (0x82) — 11 bytes:**
```
Byte 0:   0x20
Byte 1:   0x82
Byte 2:   phase (0-5, see §5.1)
Byte 3-4: chunks_received (uint16_le)
Byte 5-6: total_chunks (uint16_le)
Byte 7-10: app_version (uint32_le)
```

---

## 4. Downlink Protocol

All downlinks must fit within the **19-byte LoRa MTU**. Larger payloads are silently
dropped by the Sidewalk stack.

### 4.1 Charge Control (0x10)

4 bytes. Controls the charging relay and optional auto-resume timer.

```
Byte 0:   0x10  (CHARGE_CONTROL_CMD_TYPE)
Byte 1:   charge_allowed (0=pause, non-zero=allow)
Byte 2-3: duration_min (uint16_le, auto-resume after N minutes; 0=indefinite)
```

```c
typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;        /* 0x10 */
    uint8_t  charge_allowed;  /* 0=pause, 1=allow */
    uint16_t duration_min;    /* auto-resume after N minutes (0=indefinite) */
} charge_control_cmd_t;
```

**Auto-resume logic**: When `duration_min > 0` and `charge_allowed == 0` (pause),
`charge_control_tick()` checks `(now_ms - pause_timestamp_ms) / 60000 >= duration_min`
on each poll cycle (500ms). When true, charging is automatically re-allowed.

### 4.2 TIME_SYNC (0x30)

9 bytes. Provides wall-clock time and data acknowledgment to the device.

```
Byte 0:   0x30  (TIME_SYNC_CMD_TYPE)
Byte 1-4: epoch (uint32_le, SideCharge epoch seconds since 2026-01-01)
Byte 5-8: ack_watermark (uint32_le, latest uplink timestamp cloud has stored)
           0x00000000 = no ACK (first sync after boot)
```

**Trigger conditions:**

| Trigger | Source | Frequency |
|---------|--------|-----------|
| Device sends timestamp=0 | `decode_evse_lambda.py` → `maybe_send_time_sync()` | On each unsynced uplink |
| Daily drift correction | `decode_evse_lambda.py` sentinel check (>24h since last sync) | ~1x/day |

The TIME_SYNC sentinel is stored in DynamoDB with `device_id` + `timestamp=-2`.

### 4.3 OTA Commands (0x20)

Cloud→device subtypes:

**OTA_START (0x01) — 18-19 bytes:**
```
Byte 0:     0x20  (cmd type)
Byte 1:     0x01  (subtype)
Byte 2-5:   total_size (uint32_le, full image size in bytes)
Byte 6-7:   total_chunks (uint16_le, number of chunks to send)
Byte 8-9:   chunk_size (uint16_le, bytes per chunk — typically 15)
Byte 10-13: crc32 (uint32_le, expected CRC32 of full image)
Byte 14-17: app_version (uint32_le)
Byte 18:    flags (optional, 0x01 = OTA_START_FLAGS_SIGNED)
```

**OTA_CHUNK (0x02) — 4B header + data:**
```
Byte 0:   0x20
Byte 1:   0x02
Byte 2-3: chunk_idx (uint16_le, absolute chunk index)
Byte 4+:  data (variable, typically 15 bytes to fill the 19B LoRa MTU)
```

No per-chunk CRC — AEAD provides link-layer integrity. The full image CRC32 validates
the complete assembly after all chunks are received.

**OTA_ABORT (0x03) — 2 bytes:**
```
Byte 0: 0x20
Byte 1: 0x03
```

---

## 5. OTA System

### 5.1 State Machine

```
                       START msg
  IDLE ──────────────────────────────► RECEIVING
   ▲                                      │
   │  abort / error                       │ all chunks received
   │                                      ▼
   │                                  VALIDATING
   │                                      │
   │  CRC mismatch ──► ERROR              │ CRC OK (+signature if signed)
   │                     │                ▼
   │                     │            COMPLETE ── 15s delay ──► APPLYING ──► reboot
   │                     │                                        │
   └─────────────────────┘                                        │
                                                                  │
                                           power loss here ──► recovery on next boot
```

| Phase | Enum | Description | Duration |
|-------|------|-------------|----------|
| IDLE | 0 | Waiting for START from cloud | Indefinite |
| RECEIVING | 1 | Chunks arriving via LoRa downlinks (15B each, 19B total) | Minutes to hours |
| VALIDATING | 2 | CRC32 computed over staged image; ED25519 signature checked if signed | <1 second |
| COMPLETE | 3 | Validation passed. COMPLETE uplink sent. 15s delay before apply | Exactly 15 seconds |
| APPLYING | 4 | Copying staging→primary, page by page (4KB pages). Recovery metadata updated per page | <5 seconds |
| ERROR | 5 | Failure (CRC, flash, signature). Returns to IDLE on next START | Until new START |

**The 15-second apply delay** lets the COMPLETE uplink transmit via LoRa before the
device reboots. Without it, the cloud would never learn the OTA succeeded. An ABORT
can still cancel during this window.

### 5.2 Chunk Protocol

- **Chunk data size**: 15 bytes (configurable via `OTA_CHUNK_SIZE` env var in Lambda)
- **Total chunk wire size**: 19 bytes (4B header + 15B data = LoRa MTU)
- **Full image chunks**: ceil(image_size / chunk_size). For a 4KB app: ~274 chunks
- **Transfer time**: Each chunk takes ~15 seconds (downlink scheduling + ACK). Full 4KB
  image: ~69 minutes. Delta with 3 changed chunks: ~40 seconds.

### 5.3 Delta Mode

Delta OTA sends only chunks that differ between the current firmware (baseline) and the
new firmware. The delta logic lives entirely in the Lambda; the device doesn't need
baseline knowledge.

**Lambda side:**
1. Load baseline from S3 (`s3://evse-ota-firmware-dev/ota/baseline.bin`)
2. Compare baseline against new firmware, chunk by chunk
3. Build list of changed chunk indices (absolute, 0-indexed)
4. In OTA_START: `total_chunks = len(delta_list)` (fewer than full image)
5. In OTA_CHUNK loop: iterate delta_list, send only changed chunks with their absolute index

**Device side:**
- OTA_CHUNK carries an **absolute** chunk_idx regardless of delta/full mode
- Device writes each chunk to staging at `OTA_STAGING_ADDR + (idx × chunk_size)`
- Unchanged chunks in staging contain stale data from the last OTA (or 0xFF if erased)
- After receiving all delta chunks: device validates CRC32 over the **full** staged image
  (delta relies on unchanged chunks matching the baseline already in primary)

**Baseline capture**: `ota_deploy.py baseline` reads the device's primary partition via
pyOCD, trims trailing 0xFF, and uploads to S3.

### 5.4 Recovery Metadata

**Address**: `0xCFF00` (256 bytes between app primary and staging)

```c
struct ota_metadata {
    uint32_t magic;          /* 0x4F544155 = "OTAU" */
    uint8_t  state;          /* 0x00=NONE, 0x01=STAGED, 0x02=APPLYING */
    uint8_t  reserved[3];
    uint32_t image_size;     /* size of staged image */
    uint32_t image_crc32;    /* expected CRC32 */
    uint32_t app_version;    /* version from OTA_START */
    uint32_t pages_copied;   /* progress tracking for apply */
    uint32_t total_pages;    /* total 4KB pages to copy */
};
```

Total struct size: 28 bytes. Written as a single flash write.

**Recovery flow** (on `ota_boot_recovery_check()` at boot):

| Metadata State | Boot Action |
|----------------|-------------|
| No valid magic | Normal boot |
| `NONE` (0x00) | Normal boot |
| `STAGED` (0x01) | Clear metadata, normal boot (image was staged but never applied) |
| `APPLYING` (0x02) | **Resume** page copy from `pages_copied`, verify magic after, reboot |

### 5.5 Image Signing

ED25519 signature (64 bytes) appended to `app.bin` before upload to S3.

- **Private key**: `~/.sidecharge/ota_signing.key` (developer machine, never committed)
- **Public key**: 32-byte constant in `src/ota_signing.c` (compiled into platform)
- **Verification flow**: CRC32 validated first → then ED25519 signature verified over the
  image bytes (excluding the 64-byte signature itself) → then apply proceeds
- **Key rotation**: Requires platform reflash (acceptable for small fleet)
- **OTA_START flag**: If byte 18 has `OTA_START_FLAGS_SIGNED` (0x01), the device expects
  and verifies a signature. Without the flag, signature verification is skipped.

### 5.6 OTA Status Codes

| Code | Name | Meaning |
|------|------|---------|
| 0 | OK | Success |
| 1 | CRC_ERR | CRC32 mismatch after validation |
| 2 | FLASH_ERR | Flash write/erase failed |
| 3 | NO_SESSION | Device has no active OTA session (lost power during RECEIVING) |
| 4 | SIZE_ERR | Image too large for partition (>256KB) |
| 5 | SIG_ERR | ED25519 signature verification failed |

### 5.7 Cloud Side (ota_sender_lambda)

**Triggers:**
1. S3 PutObject: new firmware uploaded → start OTA session
2. Async invoke from decode Lambda: device ACK received → send next chunk

**Session state** in DynamoDB sentinel (`device_id` + `timestamp=-1`):
- `ota_state`: current phase
- `firmware_key`: S3 key of firmware binary
- `delta_chunks`: JSON list of changed chunk indices (null = full mode)
- `delta_cursor`: current position in delta_chunks
- `chunks_sent`: count of chunks sent
- `total_chunks`: total to send
- `retries`: consecutive retries on current chunk
- `restart_count`: NO_SESSION restart count

**Retry logic:**
- Max 5 retries per chunk (`MAX_RETRIES`)
- On `NO_SESSION` (code 3): resend OTA_START to re-establish session, up to 3 restarts
- After max retries or restarts: abort the OTA session

---

## 6. EVSE Domain Logic

### 6.1 J1772 State Machine

The J1772 pilot signal is a +/- 12V square wave whose DC level indicates vehicle state.
The ADC reads the peak voltage after a resistor divider scales it to 0-3.3V.

| State | Enum | Pilot Voltage | Meaning |
|-------|------|---------------|---------|
| A | 0 | >2600 mV | Not connected (+12V) |
| B | 1 | 1850–2600 mV | Connected, not ready (+9V) |
| C | 2 | 1100–1850 mV | Charging (+6V) |
| D | 3 | 350–1100 mV | Charging, ventilation required (+3V) |
| E | 4 | 0–350 mV | Error, short circuit (0V) |
| F | 5 | — | Error, no pilot (-12V) |
| UNKNOWN | 6 | — | ADC read failure or out-of-range |

**Voltage thresholds** (at ADC input, with implicit hysteresis from the threshold gaps):
```c
#define J1772_THRESHOLD_A_B_MV    2600
#define J1772_THRESHOLD_B_C_MV    1850
#define J1772_THRESHOLD_C_D_MV    1100
#define J1772_THRESHOLD_D_E_MV     350
```

Classification logic in `evse_sensors.c`:
```
mv > 2600 → A
mv > 1850 → B
mv > 1100 → C
mv >  350 → D
else      → E
```

State F is not detected via ADC thresholds (negative voltage); it would appear as E or
UNKNOWN in practice.

**Simulation mode**: `evse_sensors_simulate_state(state, duration_ms)` overrides real ADC
readings for the specified duration. Used for commissioning verification and shell testing.
Returns synthetic voltages (A=2980, B=2234, C=1489, D=745 mV).

### 6.2 Current Clamp

ADC channel 1 reads a current transformer output scaled to 0-3.3V = 0-30A.

```c
#define CURRENT_CLAMP_MAX_MA      30000
#define CURRENT_CLAMP_VOLTAGE_MV  3300

current_ma = (adc_mv × 30000) / 3300
```

**Change detection threshold**: Current is treated as binary on/off at 500 mA
(`CURRENT_ON_THRESHOLD_MA`). Transitions across this threshold trigger an uplink.

### 6.3 Charge Control

Controls a relay via GPIO pin 0 (`EVSE_PIN_CHARGE_EN`):
- GPIO HIGH = charging allowed (relay closed)
- GPIO LOW = charging paused (relay open)

State tracked in `charge_control_state_t`:
```c
typedef struct {
    bool     charging_allowed;     /* current relay state */
    uint16_t auto_resume_min;      /* minutes until auto-resume (0=indefinite) */
    int64_t  pause_timestamp_ms;   /* uptime when pause started */
} charge_control_state_t;
```

**Auto-resume**: When paused with `duration_min > 0`, `charge_control_tick()` checks on
each 500ms poll cycle whether the duration has elapsed. If so, it sets
`charging_allowed = true` and drives the GPIO high.

**Command sources**: Charge control can be set by:
1. Cloud downlink (0x10 command) — processed in `app_rx.c`
2. Shell command (`app evse allow` / `app evse pause`)
3. Auto-resume timer expiry

### 6.4 Thermostat Inputs

Two GPIO inputs detect HVAC demand from a thermostat:

| GPIO Pin | Flag | Bit | Meaning |
|----------|------|-----|---------|
| Pin 1 | (reserved) | 0x01 | Reserved for future heat pump support (always 0 in v1.0) |
| Pin 2 | `THERMOSTAT_FLAG_COOL` | 0x02 | Cool demand active |

`thermostat_flags_get()` returns the cool call bit. Changes trigger an uplink. The heat
call GPIO (Pin 1) is physically wired but not read in v1.0 — heat pump support is a
future extension.

### 6.5 Self-Test

Two phases: boot-time checks and continuous monitoring.

**Boot self-test** (`selftest_boot()`, <100ms):
1. ADC pilot channel readable and returns a value in range
2. ADC current channel readable
3. GPIO cool call readable
4. GPIO charge enable writable (write + readback verify)

Result is stored in `selftest_boot_result_t`. If any check fails, `FAULT_SELFTEST`
(0x80) is latched in the fault flags and included in every subsequent uplink until
explicitly reset.

**Continuous fault monitors** (`selftest_continuous_tick()`, called every 500ms):

| Monitor | Flag | Condition |
|---------|------|-----------|
| Sensor fault | 0x10 | J1772 state == UNKNOWN for >5s (ADC read failure or pilot signal out of valid range) |
| Clamp mismatch | 0x20 | Current >500mA but J1772 state != C/D, **or** current <500mA but state == C/D |
| Interlock fault | 0x40 | charge_allowed=true but conditions suggest relay isn't working |
| Self-test fail | 0x80 | Boot self-test failed (latched) |

Fault flags are OR'd into byte 5 (bits 4-7) of every uplink.

### 6.6 Event Buffer

Ring buffer of 50 timestamped EVSE state snapshots. RAM-only (no flash persistence).

```c
struct event_snapshot {           /* 10 bytes */
    uint32_t timestamp;           /* SideCharge epoch */
    uint16_t current_ma;
    uint8_t  j1772_state;         /* 0-6 */
    uint8_t  thermostat_flags;
    uint8_t  charge_flags;        /* bit 0: CHARGE_ALLOWED */
    uint8_t  _reserved;
};
```

**RAM cost**: 50 x 10 = 500 bytes + 6 bytes overhead = **506 bytes** (6.2% of 8KB budget)

**Write**: A snapshot is added on every 500ms poll cycle (`event_buffer_add()`).

**Trim**: When a TIME_SYNC downlink arrives with an ACK watermark,
`event_buffer_trim(watermark)` removes all entries with `timestamp <= watermark`.
Entries are time-ordered, so trimming walks from the tail forward and stops at the
first entry newer than the watermark.

**Overflow**: When count reaches capacity (50) and a new write arrives, the oldest entry
is overwritten. This is the designed overflow strategy — if the cloud hasn't ACK'd in
hours, old data is less valuable than current state.

**Capacity**: At 15-minute heartbeat intervals, 50 entries cover ~12.5 hours.

---

## 7. Time Sync

### 7.1 SideCharge Epoch

**Base**: 2026-01-01T00:00:00 UTC = Unix timestamp `1767225600`

All device-side timestamps use this epoch to fit in a `uint32_t` with meaningful resolution.
A SideCharge epoch value of 0 means "not synced" — the device has not received a
TIME_SYNC since boot.

```python
# Python conversion
SIDECHARGE_EPOCH = 1767225600
sidecharge_to_unix = lambda sc: sc + SIDECHARGE_EPOCH
unix_to_sidecharge = lambda unix: int(unix) - SIDECHARGE_EPOCH
```

### 7.2 Device-Side Time Derivation

The device stores a sync point and derives current time from uptime delta:

```c
/* State (13 bytes RAM) */
uint32_t sync_time;        /* SideCharge epoch at last TIME_SYNC */
uint32_t sync_uptime_ms;   /* uptime_ms() when TIME_SYNC received */
uint32_t ack_watermark;    /* latest timestamp cloud has ACK'd */
bool     synced;           /* true after first TIME_SYNC */

/* Current time computation */
uint32_t time_sync_get_epoch(void) {
    if (!synced) return 0;
    uint32_t elapsed_ms = uptime_ms() - sync_uptime_ms;
    return sync_time + (elapsed_ms / 1000);
}
```

`uptime_ms()` is a 32-bit counter that wraps at ~49 days. Since TIME_SYNC re-syncs
daily, the wrap is not a practical concern.

### 7.3 Drift and Re-Sync

**nRF52840 RTC drift**: +/-100 ppm on 32.768 kHz crystal = +/-8.6 seconds/day.

Re-sync triggers:
- **First uplink after boot**: Device sends `timestamp=0`, decode Lambda detects this
  and responds with TIME_SYNC immediately
- **Daily**: Decode Lambda checks the TIME_SYNC sentinel; if >24h since last sync,
  sends a new TIME_SYNC

After 24h without re-sync, worst-case drift is ~8.6 seconds — well within the
5-minute accuracy target.

### 7.4 ACK Watermark

The watermark field in TIME_SYNC tells the device which data the cloud has successfully
stored. The device uses it to trim old entries from the event buffer
(`event_buffer_trim(watermark)`), freeing space for new snapshots.

The watermark is typically set to the current SideCharge epoch (meaning "I've received
everything up to now").

---

## 8. AWS Cloud

### 8.1 decode_evse_lambda

This is the single ingress point for all device-to-cloud messages. Every Sidewalk
uplink hits this Lambda first: OTA traffic is forwarded to the OTA sender, telemetry
is decoded and stored, and a time-sync downlink is sent if the device clock is stale.

**Trigger**: IoT Wireless rule (Sidewalk uplink arrives)

**Flow**:
1. Base64-decode the Sidewalk payload
2. Check for OTA uplink (cmd type 0x20) → forward async to ota_sender Lambda
3. Try raw EVSE decode (magic 0xE5) → v0x06 or v0x07
4. Fall back to legacy sid_demo format
5. Store decoded telemetry in DynamoDB (`sidewalk-v1-device_events_v2`)
6. Call `maybe_send_time_sync()` — sends TIME_SYNC if sentinel is >24h old or missing
7. Update device registry (best-effort)

**DynamoDB item structure** (EVSE telemetry):
```
device_id:     wireless device ID (partition key)
timestamp:     Unix timestamp in milliseconds (sort key)
event_type:    "evse_telemetry"
data.evse:     {format, version, pilot_state, current_draw_ma,
                thermostat_bits, thermostat_cool_active,
                charge_allowed, charge_now, fault_*, device_timestamp_epoch,
                device_timestamp_unix}
link_type:     LoRa/BLE/FSK
rssi:          signal strength
seq:           Sidewalk sequence number
```

### 8.2 charge_scheduler_lambda

**Trigger**: EventBridge schedule (periodic, e.g., every 15 minutes)

**Decision logic**:
1. Check Xcel Colorado TOU peak: weekdays 5-9 PM Mountain Time
2. Query WattTime MOER signal for PSCO region
3. If TOU peak **or** MOER > threshold (default 70%): pause charging
4. Otherwise: allow charging

**State tracking**: Sentinel key (`device_id` + `timestamp=0`) stores last command.
Skips downlink if command hasn't changed (avoids redundant transmissions).

**Downlink**: Sends charge control command (0x10) with `duration_min=0` (indefinite).

### 8.3 ota_sender_lambda

This is the most stateful Lambda in the cloud triad (§8.1 decode, §8.2 charge
scheduler, §8.3 OTA sender). It manages a multi-step firmware update session over
an unreliable LoRa link where any message — downlink or uplink — can be silently
dropped. The retry logic is designed around this reality: resilience matters more
than throughput, and the 19-byte LoRa MTU means each chunk is tiny.

**Two triggers:**

1. **S3 upload** — When new firmware is uploaded to the S3 bucket, S3 fires a
   PutObject event that invokes this Lambda. It kicks off an OTA session by sending
   an `OTA_START` downlink to the device.
2. **Device ACK** — When the device acknowledges a chunk (uplink decoded by the
   decode Lambda), the decode Lambda async-invokes this one again to send the next
   chunk.

The flow is a ping-pong: **sender → device → decode → sender → device → …** until
all chunks are delivered. Each round-trip is dominated by LoRa latency (uplink +
downlink windows), not compute.

**Delta OTA:**

Rather than sending the entire firmware image, the Lambda compares the S3 **baseline**
(captured via `ota_deploy.py baseline`) against the new firmware chunk-by-chunk. Only
**changed chunks** are sent, each tagged with its absolute index so the device can
write it to the correct offset in staging flash. This is critical for keeping update
times practical — a full-image OTA sends all ~276 chunks (~69 minutes), while a
typical one-function code change sends 2–3 chunks (~5 minutes). See §5.3 for the
device-side delta merge logic.

**Session state:**

Progress is tracked in DynamoDB using a **sentinel record** — same `device_id` as
partition key but with a special `timestamp = -1` sort key (see §8.4). This record
holds the current phase, which chunks have been ACK'd, the S3 firmware key, and
delta info. See §5.7 for the full list of session state fields.

**Retry and recovery:**

- **Per-chunk**: Up to 5 retries before aborting the session.
- **Session restart**: If the device responds with `NO_SESSION` (error code 3 —
  meaning it rebooted or lost OTA state mid-session), the Lambda resends `OTA_START`
  and restarts the session from scratch, up to 3 times. This handles the common case
  of a device power-cycling during a long update.
- **Timeout**: An EventBridge rule re-invokes the Lambda if no ACK arrives within a
  timeout window (30s stale threshold, 1-minute retry interval), so the process
  doesn't stall if a downlink or uplink gets lost over LoRa.

### 8.4 DynamoDB Schema

**Table**: `sidewalk-v1-device_events_v2`

| Key | Type | Description |
|-----|------|-------------|
| `device_id` | String (PK) | AWS IoT Wireless device ID |
| `timestamp` | Number (SK) | Unix ms for events; sentinel values for state |

**Sentinel keys** (reserved timestamp values):

| Timestamp | Purpose |
|-----------|---------|
| 0 | Charge scheduler state (last command, reason, TOU/MOER) |
| -1 | OTA session state (phase, chunks, firmware key, delta info) |
| -2 | TIME_SYNC state (last sync time, epoch) |

### 8.5 Infrastructure

All AWS infrastructure is managed via Terraform (`aws/terraform/`). Never use
`aws lambda update-function-code` directly — always `terraform apply`.

Components:
- IoT Wireless (Sidewalk destination + rule)
- 3 Lambda functions (decode, scheduler, OTA sender)
- DynamoDB table (`sidewalk-v1-device_events_v2`)
- S3 bucket (`evse-ota-firmware-dev`) for firmware binaries and baselines
- EventBridge rules (scheduler schedule + OTA retry timer)
- CloudWatch alarms

---

## 9. Hardware and GPIO

### 9.1 Pin Mapping

| Abstract Pin | nRF52840 Pin | Physical Function | Direction | Usage |
|-------------|-------------|-------------------|-----------|-------|
| GPIO 0 | P0.06 | `charge_enable` | Output | Relay control (HIGH=allow, LOW=pause) |
| GPIO 1 | P0.04 | `heat_call` | Input | Thermostat heat demand (pull-down, active-high) |
| GPIO 2 | P0.05 | `cool_call` | Input | Thermostat cool demand (pull-down, active-high) |

| ADC Channel | nRF52840 Pin | Function | Range | Calibration |
|-------------|-------------|----------|-------|-------------|
| 0 | P0.02 (AIN0) | J1772 pilot voltage | 0–3300 mV | Direct millivolt reading |
| 1 | P0.03 (AIN1) | Current clamp output | 0–3300 mV | Linear: 0mV=0A, 3300mV=30A |

### 9.2 Flash Constraints

- **Page size**: 4096 bytes (nRF52840 NVMC)
- **Write alignment**: 4-byte aligned address AND length required
- **Padding**: `ota_flash_write()` pads unaligned writes with 0xFF on both sides
- **Erase**: Page-granularity only (4KB minimum)
- **Endurance**: 10,000 erase cycles per page (NVMC spec)

### 9.3 Radio

- **LoRa only**: BLE disabled for EVSE monitor (`CONFIG_SIDEWALK_LINK_MASK_LORA=y`)
- **Downlink MTU**: 19 bytes (larger payloads silently dropped)
- **TCXO**: The RAK4631's SX1262 uses an external temperature-compensated crystal oscillator
  (TCXO) for stable LoRa frequency synthesis. The TCXO is powered via the SX1262's DIO3 pin
  (1.8V, 2ms startup). The Sidewalk SDK defaults to `SX126X_TCXO_CTRL_NONE` (plain crystal),
  so without the patch the TCXO never powers on and LoRa silently fails.
  Applied via west patch: `0001-sidewalk-rak4631-tcxo-settings.patch`

---

## 10. Build and Flash

### 10.1 Toolchain

All firmware builds require NCS v2.9.1 via nrfutil toolchain-manager.

### 10.2 Platform Build

```bash
nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
  "cd /Users/emilyf/sidewalk-projects && west build -p -b rak4631 \
   rak-sid/app/rak4631_evse_monitor/ -- -DOVERLAY_CONFIG=lora.conf"
```

Output: `build/merged.hex` (576KB platform + API table at 0x8FF00)

### 10.3 App Build

```bash
nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
  "rm -rf build_app && mkdir build_app && cd build_app && \
   cmake ../rak-sid/app/rak4631_evse_monitor/app_evse && make"
```

Output: `build_app/app.hex` (~4KB actual)

### 10.4 Tests

```bash
# Host-side C unit tests (app layer, Grenning dual-target)
make -C rak-sid/app/rak4631_evse_monitor/tests/ clean test

# Lambda Python tests
python3 -m pytest rak-sid/aws/tests/ -v
```

### 10.5 Flash Procedures and Safety

```bash
# All three partitions (MFG + platform + app):
bash rak-sid/app/rak4631_evse_monitor/flash.sh all

# App only (fast, ~20KB):
bash rak-sid/app/rak4631_evse_monitor/flash.sh app

# Direct pyOCD:
/Users/emilyf/sidewalk-env/bin/pyocd flash --target nrf52840 <path-to-hex>
```

**Critical safety rules:**

1. **Platform flash erases HUK** — PSA crypto keys are re-derived on next boot.
   Safe flash order: MFG first → platform → app.

2. **Never chip-erase without backing up Sidewalk keys:**
   ```bash
   pyocd cmd -c "savemem 0xF8000 0x7000 sidewalk_keys.bin"
   ```

3. **Reboot after full flash** — first boot does BLE registration, LoRa won't connect
   until second boot.

4. **pyOCD halt corrupts BLE** — always `pyocd reset` after any halt, don't just resume.

---

## 11. Shell Commands

All shell commands are accessed via USB serial console at 115200 baud.

### 11.1 EVSE Commands (app layer, via `on_shell_cmd`)

| Command | Description |
|---------|-------------|
| `app evse status` | J1772 state, voltage, current, charge allowed, simulation active |
| `app evse a/b/c/d/e/f` | Simulate J1772 state for 10 seconds |
| `app evse allow` | Enable charging relay |
| `app evse pause` | Disable charging relay |
| `app evse buffer` | Event buffer count, oldest/newest timestamps |

### 11.2 HVAC Commands

| Command | Description |
|---------|-------------|
| `app hvac status` | Thermostat cool call flag |

### 11.3 Sidewalk and System Commands (platform layer)

| Command | Description |
|---------|-------------|
| `sid status` | Sidewalk connection state, app image status |
| `sid ota status` | OTA phase (idle/receiving/validating/applying/complete/error) |
| `sid ota report` | Send OTA_STATUS uplink |
| `app sid send` | Trigger manual uplink |
| `app sid time` | Time sync status (epoch, watermark, time since sync) |
| `app selftest` | Run commissioning self-test and print results |

---

## Appendix A: ADR Index

Architecture Decision Records are immutable once accepted. See `docs/adr/README.md`.

| ADR | Decision | Status |
|-----|----------|--------|
| [ADR-001](adr/001-version-mismatch-hard-stop.md) | API version mismatch is a hard stop (not a warning). Platform refuses to load app if version != APP_CALLBACK_VERSION. | Accepted 2026-02-11 |

## Appendix B: Known Issues

Active tracking document at `docs/known-issues.md`.

| ID | Summary | Status |
|----|---------|--------|
| KI-001 | Version mismatch → platform-only mode | Resolved (match images) |
| KI-002 | PSA AEAD error -149 after platform reflash (HUK invalidated) | Resolved (flash order: MFG→platform→app) |
| KI-003 | Stale flash data inflates OTA delta baselines | Plan drafted, not approved |

## Appendix C: SDK and Patches

Built on Nordic nRF Connect SDK v2.9.1 with the Sidewalk add-on.

**Custom vs. SDK code:**
- `sidewalk.c` (79 lines): 100% SDK standard — thread-based event dispatch
- `sidewalk_events.c` (441 lines): ~90% SDK — added MFG key health check and init tracking
- `app.c` (652 lines): ~30% SDK — split-image discovery, OTA intercept, shell dispatch
- App layer: entirely custom (1,719 lines across 11 modules)

**Patch**: `0001-sidewalk-rak4631-tcxo-settings.patch` (40 lines, applied via `west patch`)
- **Why**: The SDK hardcodes `SX126X_TCXO_CTRL_NONE` in `app_subGHz_config.c`, assuming a
  plain crystal. The RAK4631 has an external TCXO that must be powered via DIO3 — without
  this patch, the radio has no clock and LoRa TX/RX fails silently. No Kconfig option exists
  for this setting, so a source patch is the only option.
- Enables TCXO control via DIO3 for RAK4631's SX1262
- Sets 1.8V control voltage and 2ms startup timeout
- Board-conditional (`#if defined(CONFIG_BOARD_RAK4631)`)
