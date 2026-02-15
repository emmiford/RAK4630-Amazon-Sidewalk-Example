# RAK Firmware Technical Design Document

**Author:** Utz (UX Architect)
**Date:** 2026-02-14
**Scope:** RAK-side firmware only (platform + app). AWS infrastructure excluded.

---

## 1. Executive Summary

The RAK4631 EVSE Monitor firmware uses a **split-image architecture**: a large, stable **platform** image (576KB, requires physical programmer) and a small, OTA-updatable **app** image (~4KB, over LoRa). They communicate through function pointer tables at fixed flash addresses — a clean, version-enforced contract.

The architecture is fundamentally sound. The split-image idea is the right choice for a constrained LoRa device. But the implementation has accumulated boundary violations, duplicated responsibilities, and naming confusion that make the code harder to understand and modify than it needs to be.

This document maps what exists, then proposes targeted changes prioritizing **simplicity**, **cohesion**, and **readability**.

---

## 2. Current Architecture

### 2.1 Flash Layout

```
0x00000 ┌──────────────────────────┐
        │                          │
        │   Platform Image         │  576KB
        │   (Zephyr + Sidewalk +   │  - RTOS, BLE/LoRa stack
        │    drivers + OTA engine) │  - Hardware abstraction
        │                          │  - Shell infrastructure
0x8FF00 ├──────────────────────────┤
        │   Platform API Table     │  256B  (magic "PLAT", version 3)
0x90000 ├──────────────────────────┤
        │   App Callback Table     │  (magic "SAPP", version 3)
        │   App Image              │  ~4KB actual
        │   (EVSE domain logic)    │  256KB partition
0xD0000 ├──────────────────────────┤
        │   OTA Staging            │  148KB (incoming image)
0xCFF00 │   OTA Metadata           │  256B  (recovery state)
0xF5000 ├──────────────────────────┤
        │   Settings               │  8KB   (Zephyr NVS)
0xF7000 ├──────────────────────────┤
        │   HUK                    │  4KB   (PSA crypto key)
0xF8000 ├──────────────────────────┤
        │   Sidewalk Storage       │  28KB  (session keys)
0xFF000 ├──────────────────────────┤
        │   MFG Credentials        │  4KB   (device identity)
        └──────────────────────────┘
```

### 2.2 The Contract: platform_api.h

The entire relationship between platform and app is defined in one header (119 lines). The platform publishes a struct of 30 function pointers at `0x8FF00`. The app publishes a struct of 7 callbacks at `0x90000`. Both carry magic numbers and version fields validated at boot.

```
Platform provides to App:          App provides to Platform:
─────────────────────────          ──────────────────────────
send_msg()                         init(api)
is_ready()                         on_ready(bool)
get_link_mask()                    on_msg_received(data, len)
set_link_mask()                    on_msg_sent(id)
factory_reset()                    on_send_error(id, err)
adc_read_mv(channel)               on_timer()
gpio_get(pin)                      on_shell_cmd(cmd, args, print, error)
gpio_set(pin, val)
led_set(id, on)
uptime_ms()
reboot()
set_timer_interval(ms)
log_inf/err/wrn()
shell_print/error()
malloc/free()
mfg_get_version/dev_id()
```

This is the strongest part of the design. Version mismatch is a hard stop (ADR-001). The app image has zero Zephyr dependencies — it links against `libc_nano` and nothing else.

### 2.3 Build System

| Build | Toolchain | System | Output |
|-------|-----------|--------|--------|
| Platform | NCS v2.9.1 (Zephyr) | CMake + Zephyr | merged.hex (576KB) |
| App | arm-zephyr-eabi-gcc | CMake (custom) | app.hex (~4KB) |
| Tests | Native cc (macOS) | Makefile | test_app (host binary) |

The app build is completely decoupled from Zephyr. The test build compiles the same app sources against a mock platform. This is the Grenning dual-target pattern and it works well — 57 tests run in milliseconds on the host.

---

## 3. Component Map

### 3.1 Platform Layer (src/)

| File | Lines | Responsibility |
|------|-------|---------------|
| `main.c` | 51 | Hardware init (UICR, USB), calls `app_start()` |
| `app.c` | 652 | **God object.** Timer, Sidewalk lifecycle, shell dispatch, OTA routing, BLE GATT auth, app discovery |
| `sidewalk.c` | 78 | Event thread + message queue (clean) |
| `sidewalk_events.c` | 441 | Sidewalk SDK callbacks. Link switch has duplicate logic |
| `platform_api_impl.c` | 462 | Implements the 30 function pointers. ADC/GPIO lazy init |
| `ota_update.c` | 1,160 | **Largest file.** Flash alignment, CRC32, state machine, recovery, signing, delta mode, test helpers |
| `ota_signing.c` | 43 | ED25519 verify — **stub, always returns success** |
| `mfg_health.c` | 50 | Detects zeroed keys post-chip-erase (clean, isolated) |
| `app_tx.c` | 50 | TX state holder + stub `send_evse_data()`. Only used in platform-only mode |
| `app_leds.c` | 60 | LED control wrapper (clean) |
| `sid_shell.c` | 253 | Sidewalk diagnostics shell (`sid status`, `sid mfg`) |
| `evse_shell.c` | 113 | **Boundary violation.** EVSE shell commands in platform, imports app headers |
| `hvac_shell.c` | 51 | **Boundary violation.** HVAC shell commands in platform, imports app headers |
| **Total** | **3,464** | |

### 3.2 App Layer (src/app_evse/)

| File | Lines | Responsibility |
|------|-------|---------------|
| `app_entry.c` | 352 | Entry point, callback table, lifecycle dispatch, polling loop, shell routing |
| `evse_sensors.c` | 165 | J1772 pilot ADC → state classification + current clamp scaling |
| `charge_control.c` | 113 | GPIO relay control + auto-resume timer |
| `thermostat_inputs.c` | 57 | GPIO heat/cool call reading |
| `rak_sidewalk.c` | 86 | **Misnamed.** Aggregates sensor data into payload struct |
| `app_tx.c` | 113 | 12-byte uplink encoding (magic + version + sensors + timestamp) |
| `app_rx.c` | 56 | Downlink dispatch (CHARGE_CONTROL, TIME_SYNC) |
| `event_buffer.c` | 144 | Ring buffer of 50 timestamped EVSE snapshots |
| `time_sync.c` | 108 | Cloud time sync (SideCharge epoch + ACK watermark) |
| `selftest.c` | 301 | Boot checks + 4 continuous fault monitors |
| `selftest_trigger.c` | 224 | 5-press button trigger + LED blink codes |
| **Total** | **1,719** | |

### 3.3 Tests

| File | Lines | Coverage |
|------|-------|---------|
| `test_app.c` | 954 | 57 tests across all 11 app modules |
| `mock_platform.c` | 205 | Complete platform_api mock with fault injection |
| `mock_platform.h` | 63 | Mock state struct (ADC values, GPIO, send captures) |

---

## 4. Architectural Assessment

### 4.1 What Works Well

**The split-image contract.** `platform_api.h` is 119 lines and defines the entire relationship between two independently-built firmware images. The function pointer table pattern is proven in this class of embedded systems. Version enforcement (ADR-001) prevents the most dangerous failure mode — silent memory corruption from mismatched structs.

**The Grenning dual-target test pattern.** Same source compiled against mock on the host. 57 tests run instantly. The mock supports fault injection (ADC failures, GPIO readback mismatches) which enables thorough self-test verification. This is the right testing architecture for this project.

**App module cohesion (mostly).** Most app modules have a single, clear responsibility. `evse_sensors.c` reads ADC and classifies J1772 state — nothing else. `charge_control.c` owns the relay — nothing else. `event_buffer.c` is a pure data structure with no knowledge of what it stores. These modules are easy to understand and modify in isolation.

**OTA recovery design.** Metadata-tracked page-by-page copy with power-loss resume is well-engineered for a device that can't be physically accessed. The deferred-apply pattern (15s delay for COMPLETE uplink) is pragmatic.

### 4.2 What Needs Attention

#### Problem 1: app.c is a god object (652 lines, 6+ responsibilities)

`app.c` does too many things:
- Discovers and validates the app image
- Manages the periodic timer (create, start, callback, work queue)
- Handles Sidewalk lifecycle events (status changes, message routing)
- Routes OTA messages (intercepts type 0x20 before app sees them)
- Implements platform-side shell commands (`sid status`, `sid reinit`, `sid lora`, `sid ota *`)
- Acts as shell router for app commands (`app <cmd>`)
- Handles BLE GATT authorization

This makes it the hardest file to understand and the riskiest to modify. Every change to platform behavior touches this file.

#### Problem 2: Platform/app boundary violations

Three files in the platform layer import app-layer headers:

- `evse_shell.c` includes `evse_sensors.h` and `charge_control.h`
- `hvac_shell.c` includes `thermostat_inputs.h`
- `rak_sidewalk.h` (platform include/) defines EVSE payload struct

This means the "generic Sidewalk sensor platform" actually has EVSE-specific knowledge compiled in. If you wanted to reuse the platform for a different sensor type, you'd have to gut these files. More practically, it means platform builds break if you change app-layer headers.

These shell commands are actually dead code in the platform build context. The app already handles `app evse *` and `app hvac *` through the `on_shell_cmd` callback in `app_entry.c`. The platform-side copies exist from before the shell dispatch mechanism was added and were never removed.

#### Problem 3: ota_update.c is monolithic (1,160 lines)

This single file contains:
- Flash abstraction (alignment-aware read/write/erase)
- CRC32 computation over flash regions
- OTA protocol state machine (IDLE → RECEIVING → VALIDATING → APPLYING)
- Recovery metadata (write/read/clear)
- Delta mode bitfield tracking
- Signature verification orchestration
- Deferred apply (work queue scheduling)
- Test/debug helpers

It's internally consistent, but its size makes it intimidating. A developer looking at the OTA system has to hold 1,160 lines in their head at once.

#### Problem 4: Naming confusion

| Name | What you'd expect | What it actually does |
|------|--------------------|----------------------|
| `rak_sidewalk.c` | Sidewalk protocol handling | Aggregates sensor data into a struct |
| `app_tx.c` (platform) | Transmit logic | State holder + empty stub |
| `app_tx.c` (app) | Transmit logic | Actual payload encoding |
| `evse_shell.c` (platform) | Platform shell | EVSE-specific shell (app concern) |

Two files named `app_tx.c` in different directories doing different things is a readability trap.

#### Problem 5: API distribution ceremony

`app_entry.c` calls 10 separate setter functions to distribute the platform API pointer:

```c
evse_sensors_set_api(api);
charge_control_set_api(api);
thermostat_inputs_set_api(api);
rak_sidewalk_set_api(api);
app_tx_set_api(api);
app_rx_set_api(api);
time_sync_set_api(api);
selftest_set_api(api);
selftest_trigger_set_api(api);
// ...and each module stores it in a file-scope static
```

This is 10 functions that do the same thing: `static const struct platform_api *api = ptr;`. It's not wrong, but it's ceremony — and every new module requires adding another setter call to `app_entry.c`.

#### Problem 6: selftest.c bypasses evse_sensors

`selftest_continuous_tick()` reads ADC directly via `api->adc_read_mv(0)` instead of using `evse_sensors_j1772_state_get()`. This means:
- Simulation mode doesn't affect selftest (inconsistent behavior during testing)
- Two code paths interpret the same ADC channel (maintenance risk)
- The selftest has implicit knowledge of ADC channel numbering

#### Problem 7: Duplicated shell status commands

`cmd_sid_status()` is implemented in both `app.c` (lines 352-401) and `sid_shell.c` (lines 24-73), with slightly different verbosity. It's unclear which runs when you type `sid status`.

---

## 5. Proposed Changes

Ordered by impact-to-effort ratio. Each change is independent — they can be done in any order.

### Change 1: Delete platform-side EVSE shell files

**Delete:** `src/evse_shell.c`, `src/hvac_shell.c`
**Remove from:** `CMakeLists.txt` platform source list

**Why:** These are dead code. The app already handles `app evse *` and `app hvac *` commands through the `on_shell_cmd` callback mechanism in `app_entry.c`. The platform-side copies are leftover from before that mechanism existed. Removing them eliminates the boundary violation and simplifies the platform build.

**Risk:** None. Verify by typing `app evse status` after removal — it routes through `app_entry.c`, not `evse_shell.c`.

**Effort:** Small (delete 2 files, remove 2 lines from CMakeLists.txt).

### Change 2: Move rak_sidewalk.h payload struct to app layer

**Move:** EVSE payload struct from `include/rak_sidewalk.h` to a new `include/evse_payload.h` (or inline it into `rak_sidewalk.c` if nothing outside the app uses it).

**Why:** The platform has no business defining EVSE payload formats. The platform sends raw bytes via `send_msg()` — it doesn't need to know what's in them.

**Effort:** Small (move a struct, update includes).

### Change 3: Rename rak_sidewalk.c → evse_payload.c

**Rename:** `src/app_evse/rak_sidewalk.c` → `src/app_evse/evse_payload.c`
**Rename:** `include/rak_sidewalk.h` → `include/evse_payload.h`

**Why:** The module aggregates sensor readings into a payload struct. It has nothing to do with the Sidewalk protocol. The name should describe what it does.

**Effort:** Small (rename + update includes across ~4 files).

### Change 4: Resolve the two app_tx.c files

**Option A (recommended):** Rename platform's `src/app_tx.c` → `src/tx_state.c` (or `src/sidewalk_tx_state.c`). It's a state holder for TX readiness and link mask — that's what it should be called.

**Option B:** If platform-only mode (no app image) is not a real use case, delete the platform's `app_tx.c` entirely and let `platform_api_impl.c` hold the TX state directly (it's only 50 lines).

**Why:** Two files with the same name doing different things is a trap. Developers will open the wrong one.

**Effort:** Small.

### Change 5: Replace per-module API setters with a shared pointer

Replace the 10 `*_set_api()` functions with a single shared pointer:

```c
/* app_platform.h (new, 3 lines) */
#include "platform_api.h"
extern const struct platform_api *platform;
```

```c
/* app_platform.c (new, 2 lines) */
#include "app_platform.h"
const struct platform_api *platform = NULL;
```

```c
/* app_entry.c: init becomes */
static int app_init(const struct platform_api *api) {
    platform = api;  // One assignment, all modules can see it
    evse_sensors_init();
    charge_control_init();
    // ...
}
```

```c
/* evse_sensors.c: before */
static const struct platform_api *api;
void evse_sensors_set_api(const struct platform_api *a) { api = a; }
int evse_pilot_voltage_read(void) { return api->adc_read_mv(0); }

/* evse_sensors.c: after */
#include "app_platform.h"
int evse_pilot_voltage_read(void) { return platform->adc_read_mv(0); }
```

**Why:** Eliminates 10 setter functions, 10 static pointers, and 10 lines in `app_init()`. Every module accesses the same pointer. Initialization order is still controlled by `app_init()`.

**Trade-off:** Modules are no longer independently injectable for testing. But the mock already replaces the entire platform_api struct — tests just set `platform = mock_api()` once. This is simpler, not worse.

**Effort:** Medium (touch every app module, update tests).

### Change 6: Split ota_update.c into two files

**Split into:**
- `ota_flash.c` (~150 lines): Flash init, erase, write (with alignment), read, CRC32 computation
- `ota_update.c` (~1,000 lines): State machine, protocol, recovery, delta mode, signing, apply

**Why:** The flash abstraction is a stable, reusable layer. The OTA state machine changes when the protocol evolves. Separating them means flash changes (e.g., different chip) don't risk breaking OTA logic, and OTA protocol changes don't risk breaking flash alignment.

**Effort:** Medium (extract functions, add header, update build).

### Change 7: Break up app.c

Extract focused modules from the 652-line monolith:

- **`app.c` (~200 lines)**: Boot sequence only. Discover app image, validate version, init OTA, check recovery, start Sidewalk, register timer. The "main" of the platform.
- **`platform_shell.c` (~200 lines)**: All platform shell commands (`sid status`, `sid reinit`, `sid lora/ble/reset`, `sid ota *`). Currently scattered across app.c and sid_shell.c with duplication.
- **`sidewalk_dispatch.c` (~150 lines)**: Sidewalk event handlers (`on_msg_received`, `on_status_changed`, `on_send_ok`, `on_send_error`). The glue between Sidewalk SDK events and app callbacks.

**Why:** After this, `app.c` reads as a boot script. You can understand the boot sequence without scrolling past shell commands and message handlers. Shell commands are in one place. Sidewalk dispatch is in one place.

**Also resolves:** The duplicate `cmd_sid_status()` — consolidate into the one `platform_shell.c`.

**Effort:** Large (refactor, but no behavior change — pure structural).

### Change 8: Route selftest through evse_sensors

Change `selftest_continuous_tick()` to accept sensor readings from the caller (app_entry.c already reads them in the polling loop) instead of reading ADC directly:

```c
/* Before: selftest reads ADC directly */
void selftest_continuous_tick(j1772_state_t state, int current_ma,
                              bool charge_allowed) {
    int pilot_mv = api->adc_read_mv(0);  // Bypasses evse_sensors
    // ...
}

/* After: caller provides all readings */
void selftest_continuous_tick(j1772_state_t state, int pilot_mv,
                              int current_ma, bool charge_allowed,
                              uint8_t thermostat_flags) {
    // Uses what it's given — no ADC knowledge needed
}
```

**Why:** Selftest no longer needs to know ADC channel numbers. It tests the values, not the hardware. Simulation mode works correctly. One less module holding a platform API pointer.

**Effort:** Small (change function signature, update caller and tests).

---

## 6. Proposed Dependency Structure

### Current (simplified)

```
                    app.c (god object)
                   / | | | \
                  /  | | |  \
     sidewalk.c  OTA  shell  BLE   timer
         |                  dispatch
  sidewalk_events.c          / \
                     evse_shell  hvac_shell ← BOUNDARY VIOLATION
                     (platform    (platform
                      imports      imports
                      app headers) app headers)
```

### Proposed

```
                    app.c (boot only)
                      |
           ┌──────────┼──────────────┐
           |          |              |
  sidewalk_dispatch  OTA        platform_shell
           |       /    \            |
  sidewalk.c    ota_     ota_    (all sid/ota
       |       flash   update    shell cmds)
  sidewalk_events.c
```

App layer (unchanged except cleaner API access):

```
              app_entry.c (dispatcher)
                   |
    ┌──────┬──────┬┼──────┬──────┬──────┐
    |      |      ||      |      |      |
  evse_  charge_ therm  evse_  app_   app_rx
  sensors control ostat payload  tx      |
    |                     |      |    ┌──┴──┐
    |                     |      |  charge  time
    └─────────────────────┘      |  control sync
         (reads sensors)         |          |
                                 |     event_buffer
                            (reads payload,
                             charge_control,
                             time_sync)
```

Selftest reads sensor values passed by `app_entry.c`, no direct hardware access:

```
  selftest ← receives (state, mv, mA, flags) from app_entry polling loop
  selftest_trigger ← calls selftest_boot(), reports via send callback
```

---

## 7. What I Would NOT Change

**The split-image architecture.** It's the right design for this device class. The 4KB OTA app image over LoRa is a genuine competitive advantage.

**The function pointer table contract.** Fixed-address struct with magic + version is simple, proven, and debuggable. Don't add vtables, registries, or dynamic dispatch.

**The Grenning test pattern.** Host-side tests with a mock platform are exactly right. Don't add a Zephyr test runner or hardware-in-the-loop framework unless you need integration coverage beyond the existing 7 serial E2E tests.

**The polling architecture.** 500ms timer tick is appropriate for J1772 state detection (pilot signal changes are in the 100ms-1s range). Event-driven would add complexity with no benefit at this frequency.

**The OTA recovery metadata.** Page-tracked progress with boot-time resume is the right approach for a device you can't physically reach. The 15s deferred apply is pragmatic.

**Module-per-concern in the app layer.** The app already has good cohesion — each module owns one thing. The changes above are about making the platform layer match that standard.

---

## 8. Summary of Proposed Changes

| # | Change | Effort | Impact |
|---|--------|--------|--------|
| 1 | Delete evse_shell.c + hvac_shell.c from platform | Small | Removes boundary violations |
| 2 | Move EVSE payload struct to app layer | Small | Cleans platform/app boundary |
| 3 | Rename rak_sidewalk → evse_payload | Small | Name matches responsibility |
| 4 | Resolve two app_tx.c files | Small | Eliminates naming trap |
| 5 | Shared platform pointer (replace 10 setters) | Medium | Reduces ceremony, simpler init |
| 6 | Split ota_update.c → ota_flash.c + ota_update.c | Medium | Separates stable flash layer from evolving protocol |
| 7 | Break up app.c → boot + shell + dispatch | Large | Eliminates god object |
| 8 | Route selftest through sensors, not ADC directly | Small | Consistent abstraction |

All changes are structural refactors — no behavior changes, no new features, no API version bump needed.
