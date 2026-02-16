# Telemetry Pipeline Architecture: TASK-033, TASK-034, TASK-035

**Author**: Eliel (Backend Architect)
**Date**: 2026-02-13
**Status**: Design Review
**PRD sections**: 3.2 (Uplink), 3.2.1 (Payload Format), 3.2.2 (Event Buffer), 3.3 (Downlink/TIME_SYNC)

---

## 1. TIME_SYNC Downlink (0x30) -- TASK-033

### 1.1 Wire Format

```
Byte  Offset  Field               Type            Description
----  ------  -----               ----            -----------
0     0       Command type        uint8           0x30 (TIME_SYNC)
1-4   1       Current time        uint32_le       SideCharge epoch seconds
5-8   5       ACK watermark       uint32_le       Latest uplink timestamp cloud has received
                                                  0x00000000 = no ACK (first sync after boot)
```

**Total: 9 bytes. Fits within 19-byte LoRa downlink MTU with 10 bytes to spare.**

SideCharge epoch: seconds since 2026-01-01 00:00:00 UTC. Python conversion:

```python
import time
from datetime import datetime, timezone

SIDECHARGE_EPOCH = int(datetime(2026, 1, 1, tzinfo=timezone.utc).timestamp())
# = 1767225600

def unix_to_sidecharge(unix_ts):
    return int(unix_ts) - SIDECHARGE_EPOCH

def sidecharge_to_unix(sc_ts):
    return sc_ts + SIDECHARGE_EPOCH
```

### 1.2 Device-Side State (app RAM)

```c
/* In a new module: time_sync.c / time_sync.h */

typedef struct {
    uint32_t sync_time;        /* SideCharge epoch at last sync (from cloud)      */
    uint32_t sync_uptime_ms;   /* api->uptime_ms() at moment of sync receipt      */
    uint32_t ack_watermark;    /* Latest timestamp cloud has ACK'd                */
    bool     synced;           /* true after first TIME_SYNC received             */
} time_sync_state_t;

static time_sync_state_t ts_state = { .synced = false };
```

**RAM cost**: 13 bytes.

### 1.3 Computing Current Time

```c
uint32_t time_sync_now(void)
{
    if (!ts_state.synced) {
        return 0;  /* timestamp=0 signals "not synced" to cloud */
    }
    uint32_t elapsed_ms = api->uptime_ms() - ts_state.sync_uptime_ms;
    return ts_state.sync_time + (elapsed_ms / 1000);
}
```

The `uptime_ms()` function already exists in `platform_api.h`. No new platform API needed for basic time computation. The `k_uptime_get()` underneath is a 64-bit counter, but we only care about the 32-bit delta (wraps at ~49 days -- long before any meaningful drift accumulation).

### 1.4 Processing TIME_SYNC on Device

In `app_rx.c`, add a handler for 0x30:

```c
#define TIME_SYNC_CMD_TYPE  0x30
#define TIME_SYNC_CMD_SIZE  9

void app_rx_process_msg(const uint8_t *data, size_t len)
{
    /* ... existing 0x10 handler ... */

    if (len >= TIME_SYNC_CMD_SIZE && data[0] == TIME_SYNC_CMD_TYPE) {
        uint32_t cloud_time = data[1] | (data[2] << 8) |
                              (data[3] << 16) | (data[4] << 24);
        uint32_t watermark  = data[5] | (data[6] << 8) |
                              (data[7] << 16) | (data[8] << 24);
        time_sync_process(cloud_time, watermark);
        return;
    }

    /* ... unknown command fallthrough ... */
}
```

```c
void time_sync_process(uint32_t cloud_time, uint32_t watermark)
{
    ts_state.sync_time      = cloud_time;
    ts_state.sync_uptime_ms = api->uptime_ms();
    ts_state.ack_watermark  = watermark;
    ts_state.synced         = true;

    api->log_inf("TIME_SYNC: time=%u wm=%u", cloud_time, watermark);

    /* Notify event buffer to trim entries <= watermark */
    event_buffer_trim(watermark);
}
```

### 1.5 Drift Correction Strategy

**nRF52840 RTC drift**: +/-100 ppm on 32.768 kHz crystal = +/-8.6 seconds/day.

**Strategy**: Periodic re-sync from cloud. No client-side drift compensation.

| Trigger | Action | Frequency |
|---------|--------|-----------|
| First uplink after boot (timestamp=0) | decode Lambda detects `timestamp==0` in v0x07 payload, triggers TIME_SYNC | Once per boot |
| Daily drift correction | charge_scheduler Lambda sends TIME_SYNC on first invocation after midnight UTC | ~1x/day |
| Manual re-sync | Operator invokes `sid time_sync` shell command (uplinks with timestamp=0 to force cloud sync) | On demand |

After 24 hours without re-sync, worst-case drift is ~8.6 seconds. After 7 days, ~60 seconds. Both are well within the 5-minute accuracy target from the PRD.

**No persistent storage needed**: Time state is RAM-only. On reboot, `synced=false`, timestamp=0 goes up in the first uplink, cloud responds with TIME_SYNC. Recovery time is one uplink-downlink round trip (~15-60 seconds over LoRa).

### 1.6 Lambda Code Path: Sending TIME_SYNC

Two Lambdas need changes:

**decode_evse_lambda.py** -- detect sync request, send TIME_SYNC:

```python
SIDECHARGE_EPOCH = 1767225600  # 2026-01-01 00:00:00 UTC
TIME_SYNC_CMD = 0x30

def send_time_sync(ack_watermark=0):
    """Send TIME_SYNC downlink with current time and ACK watermark."""
    now_sc = int(time.time()) - SIDECHARGE_EPOCH
    payload = bytes([TIME_SYNC_CMD]) + \
              now_sc.to_bytes(4, 'little') + \
              ack_watermark.to_bytes(4, 'little')
    send_sidewalk_msg(payload, transmit_mode=1)
    print(f"TIME_SYNC sent: time={now_sc} watermark={ack_watermark}")
```

Trigger condition in `lambda_handler()`:
```python
# After decoding v0x07 payload:
if decoded.get('format') == 'raw_v7' and decoded.get('device_timestamp', 0) == 0:
    # Device is requesting time sync (just booted, or never synced)
    send_time_sync(ack_watermark=0)
```

**charge_scheduler_lambda.py** -- periodic daily sync:

```python
def maybe_send_time_sync(last_state):
    """Send TIME_SYNC if last sync was >23 hours ago."""
    last_sync = last_state.get('last_time_sync_epoch', 0) if last_state else 0
    now = int(time.time())
    if now - last_sync > 23 * 3600:
        # Look up latest watermark from DynamoDB (most recent uplink timestamp)
        watermark = get_latest_uplink_watermark()
        send_time_sync(ack_watermark=watermark)
        return now
    return last_sync
```

### 1.7 When to Request Re-Sync

The device itself never "requests" a sync -- it cannot initiate a downlink. Instead, it signals the need by sending `timestamp=0` in its uplink payload. The cloud interprets this as "device has no time reference" and responds with TIME_SYNC.

| Device state | Uplink timestamp value | Cloud action |
|-------------|----------------------|--------------|
| Just booted, never synced | `0x00000000` | Send TIME_SYNC immediately |
| Synced, normal operation | Valid SideCharge epoch | No action (unless daily re-sync due) |
| Synced, after 24h without re-sync | Valid but drifting | charge_scheduler sends daily re-sync |

---

## 2. Event Buffer -- TASK-034

### 2.1 Ring Buffer Data Structure

```c
/* event_buffer.h */

#define EVENT_ENTRY_SIZE    12   /* same as uplink payload: 8B data + 4B timestamp */
#define EVENT_BUFFER_CAPACITY  50
#define EVENT_BUFFER_SIZE   (EVENT_ENTRY_SIZE * EVENT_BUFFER_CAPACITY)  /* = 600 bytes */

typedef struct __attribute__((packed)) {
    /* Bytes 0-7: sensor snapshot (same layout as uplink bytes 0-7) */
    uint8_t  magic;             /* 0xE5 -- redundant but useful for debug dumps  */
    uint8_t  version;           /* 0x07                                          */
    uint8_t  j1772_state;       /* J1772 state enum                              */
    uint16_t j1772_mv;          /* Pilot voltage, little-endian                  */
    uint16_t current_ma;        /* Charging current, little-endian               */
    uint8_t  thermostat_flags;  /* Full 8-bit thermostat+control+fault byte      */
    /* Bytes 8-11: device-side timestamp */
    uint32_t timestamp;         /* SideCharge epoch seconds (0 = not synced)     */
} event_entry_t;

_Static_assert(sizeof(event_entry_t) == 12, "event_entry_t must be 12 bytes");

typedef struct {
    event_entry_t entries[EVENT_BUFFER_CAPACITY];
    uint8_t  head;          /* Next write position (0..49)                       */
    uint8_t  count;         /* Number of valid entries (0..50)                   */
    uint32_t trim_watermark; /* Highest ACK'd timestamp -- entries <= this trimmed */
} event_buffer_t;
```

**RAM cost**:
- `entries`: 50 x 12 = **600 bytes**
- `head + count + trim_watermark`: **6 bytes**
- **Total: 606 bytes** from the app's 8KB (7.4% of budget)

### 2.2 Capacity Analysis

| Uplink interval | Buffer duration | Entries before wrap |
|----------------|-----------------|---------------------|
| 15 min (heartbeat only) | 12.5 hours | 50 |
| 5 min (heavy state changes) | 4.2 hours | 50 |
| 1 min (pathological chatter) | 50 minutes | 50 |

50 entries at 15-minute heartbeat covers **12.5 hours** of data -- more than enough for overnight cloud outages. If the cloud ACKs normally (every downlink opportunity), the buffer stays nearly empty.

### 2.3 Operations

**Write** (called on every uplink-worthy event):

```c
void event_buffer_write(const event_entry_t *entry)
{
    buf.entries[buf.head] = *entry;
    buf.head = (buf.head + 1) % EVENT_BUFFER_CAPACITY;
    if (buf.count < EVENT_BUFFER_CAPACITY) {
        buf.count++;
    }
    /* If count was already at capacity, oldest entry was overwritten (ring wrap) */
}
```

**Read latest** (for uplink -- sends most recent state):

```c
const event_entry_t *event_buffer_latest(void)
{
    if (buf.count == 0) return NULL;
    uint8_t idx = (buf.head + EVENT_BUFFER_CAPACITY - 1) % EVENT_BUFFER_CAPACITY;
    return &buf.entries[idx];
}
```

**Trim** (called when ACK watermark arrives via TIME_SYNC):

```c
void event_buffer_trim(uint32_t watermark)
{
    if (watermark == 0) return;  /* No ACK -- don't trim */
    buf.trim_watermark = watermark;

    /* Walk buffer and invalidate entries with timestamp <= watermark.
     * Since this is a ring buffer written in time order, we trim from
     * the tail (oldest) forward until we hit an entry newer than watermark. */
    uint8_t tail = (buf.head + EVENT_BUFFER_CAPACITY - buf.count) % EVENT_BUFFER_CAPACITY;
    uint8_t trimmed = 0;

    for (uint8_t i = 0; i < buf.count; i++) {
        uint8_t idx = (tail + i) % EVENT_BUFFER_CAPACITY;
        if (buf.entries[idx].timestamp <= watermark) {
            trimmed++;
        } else {
            break;  /* Entries are time-ordered; stop at first newer entry */
        }
    }

    buf.count -= trimmed;
    /* head stays the same; tail implicitly moves forward */
}
```

### 2.4 Buffer Overflow Behavior

When `count == EVENT_BUFFER_CAPACITY` and a new write arrives, the oldest entry is overwritten. This is the correct behavior per PRD 3.2.2:

> "If no ACK arrives, the ring buffer wraps and overwrites the oldest entries. This is acceptable -- if the cloud hasn't ACK'd in hours, old data is less valuable than current state."

No special handling needed -- the ring buffer wrap is the overflow strategy.

### 2.5 Integration with Uplink

Current flow in `app_entry.c` `app_on_timer()`:
1. Poll sensors, detect changes
2. If changed or heartbeat due, call `app_tx_send_evse_data()`

New flow:
1. Poll sensors, detect changes
2. If changed or heartbeat due:
   a. Build `event_entry_t` from current sensor state + `time_sync_now()`
   b. Write to event buffer
   c. Send the entry as the uplink payload (12 bytes)

The event buffer acts as a local log. Every uplink is also a buffer write. The buffer exists so that if a LoRa uplink is dropped, the state snapshot is preserved and can be reconstructed from future ACKs.

### 2.6 No Flash Persistence

The event buffer is RAM-only. On reboot, it starts empty. Rationale:

1. The app has no flash write API in `platform_api.h` today (adding one is possible but non-trivial -- see section 7).
2. Reboot is rare (OTA, watchdog, power loss). The first post-boot uplink with `timestamp=0` triggers TIME_SYNC, and the cloud sees the gap in the timeline.
3. Flash wear: writing 50 entries x 12 bytes every 15 minutes would wear out nRF52840 NVMC (10K cycles) within months. Not worth it.
4. The data is periodic snapshots, not financial transactions. A gap on reboot is acceptable.

---

## 3. Uplink v0x07 Format -- TASK-035

### 3.1 Byte-Level Layout

```
Offset  Size  Field               Current v0x06        New v0x07
------  ----  -----               -------------        ---------
0       1     Magic               0xE5                 0xE5 (unchanged)
1       1     Version             0x06                 0x07 (bumped)
2       1     J1772 state         enum 0-6             enum 0-6 (unchanged)
3-4     2     Pilot voltage       uint16_le mV         uint16_le mV (unchanged)
5-6     2     Current             uint16_le mA         uint16_le mA (unchanged)
7       1     Thermostat byte     bits 0-1 only        full 8-bit (see below)
8-11    4     Timestamp           (not present)        uint32_le SideCharge epoch
                                                       0 = not synced
------  ----  -----               -------------        ---------
TOTAL   8     v0x06               12 v0x07
```

**v0x07 is 12 bytes. Fits within 19-byte MTU with 7 bytes spare.**

### 3.2 Thermostat Byte (Byte 7) -- Full Bitfield

The PRD defines 8 bits for byte 7. Currently, only bits 0-1 are populated by `thermostat_flags_get()`. v0x07 populates all 8 bits:

```
Bit  Mask  Name              Source                    Current  v0x07
---  ----  ----              ------                    -------  -----
0    0x01  HEAT              thermostat_heat_call_get()  YES      YES
1    0x02  COOL              thermostat_cool_call_get()  YES      YES
2    0x04  CHARGE_ALLOWED    charge_control_is_allowed() NO       YES
3    0x08  CHARGE_NOW        charge_now_active()         NO       YES (stub: always 0 until CHARGE_NOW button impl)
4    0x10  SENSOR_FAULT      self_test fault flag        NO       YES (stub: always 0 until self-test impl)
5    0x20  CLAMP_MISMATCH    cross-check fault flag      NO       YES (stub: always 0 until self-test impl)
6    0x40  INTERLOCK_FAULT   relay effectiveness check   NO       YES (stub: always 0 until self-test impl)
7    0x80  SELFTEST_FAIL     on-demand self-test flag    NO       YES (stub: always 0 until self-test impl)
```

Concrete change: `evse_payload_get()` currently calls only `thermostat_flags_get()` which returns bits 0-1. For v0x07, it needs to OR in the charge control bit:

```c
payload.thermostat_flags = thermostat_flags_get();
if (charge_control_is_allowed()) {
    payload.thermostat_flags |= 0x04;  /* CHARGE_ALLOWED */
}
/* Bits 3-7 remain 0 until their features are implemented */
```

### 3.3 Comparison: v0x06 vs v0x07

```
v0x06 (current):  E5 06 03 34 08 D0 07 02
                  |  |  |  |     |     |
                  |  |  |  2100mV 2000mA thermostat (cool=1)
                  |  |  State C (charging)
                  |  Version 0x06
                  Magic

v0x07 (new):      E5 07 03 34 08 D0 07 06 39 A2 04 00
                  |  |  |  |     |     |  |           |
                  |  |  |  2100mV 2000mA |  SideCharge epoch timestamp
                  |  |  State C          thermostat (cool=1, charge_allowed=1)
                  |  Version 0x07
                  Magic
```

In the v0x07 example: thermostat byte is 0x06 = COOL (0x02) | CHARGE_ALLOWED (0x04). Timestamp 0x0004A239 = 304697 seconds = ~3.5 days after 2026-01-01.

### 3.4 Backward Compatibility

The decode Lambda identifies the format by the version byte (byte 1). Both v0x06 and v0x07 start with magic 0xE5. The Lambda handles both:

- `version <= 0x06`: 8-byte payload, no timestamp, thermostat bits 0-1 only
- `version == 0x07`: 12-byte payload, 4-byte timestamp, full thermostat byte

No breaking change. Old devices continue to send v0x06 and are decoded correctly.

### 3.5 app_tx.c Changes

```c
/* Updated constants */
#define EVSE_VERSION      0x07
#define EVSE_PAYLOAD_SIZE 12

int app_tx_send_evse_data(void)
{
    evse_payload_t data = evse_payload_get();
    uint32_t ts = time_sync_now();

    uint8_t payload[EVSE_PAYLOAD_SIZE] = {
        EVSE_MAGIC,
        EVSE_VERSION,
        data.j1772_state,
        data.j1772_mv & 0xFF,
        (data.j1772_mv >> 8) & 0xFF,
        data.current_ma & 0xFF,
        (data.current_ma >> 8) & 0xFF,
        data.thermostat_flags,
        ts & 0xFF,
        (ts >> 8) & 0xFF,
        (ts >> 16) & 0xFF,
        (ts >> 24) & 0xFF,
    };

    /* Also write to event buffer */
    event_entry_t entry;
    memcpy(&entry, payload, sizeof(entry));
    event_buffer_write(&entry);

    return api->send_msg(payload, sizeof(payload));
}
```

---

## 4. Decode Lambda Changes (decode_evse_lambda.py)

### 4.1 New Decoder for v0x07

```python
EVSE_VERSION_V7 = 0x07
EVSE_V7_PAYLOAD_SIZE = 12
SIDECHARGE_EPOCH = 1767225600  # 2026-01-01 00:00:00 UTC

# Thermostat byte bit definitions (byte 7)
THERM_HEAT           = 0x01
THERM_COOL           = 0x02
THERM_CHARGE_ALLOWED = 0x04
THERM_CHARGE_NOW     = 0x08
THERM_SENSOR_FAULT   = 0x10
THERM_CLAMP_MISMATCH = 0x20
THERM_INTERLOCK_FAULT = 0x40
THERM_SELFTEST_FAIL  = 0x80

def decode_raw_evse_payload(raw_bytes):
    """Decode raw EVSE payload. Handles v0x01-v0x06 (8B) and v0x07+ (12B)."""
    if len(raw_bytes) < EVSE_PAYLOAD_SIZE:  # minimum 8 bytes
        return None

    magic = raw_bytes[0]
    version = raw_bytes[1]

    if magic != EVSE_MAGIC:
        return None

    j1772_state = raw_bytes[2]
    pilot_voltage = int.from_bytes(raw_bytes[3:5], 'little')
    current_ma = int.from_bytes(raw_bytes[5:7], 'little')
    thermostat_bits = raw_bytes[7]

    if j1772_state > 6 or pilot_voltage > 15000 or current_ma > 100000:
        return None

    result = {
        'payload_type': 'evse',
        'format': f'raw_v{version}',
        'version': version,
        'j1772_state_code': j1772_state,
        'j1772_state': J1772_STATES.get(j1772_state, 'UNKNOWN'),
        'pilot_voltage_mv': pilot_voltage,
        'current_ma': current_ma,
        'thermostat_bits': thermostat_bits,
        'thermostat_heat': bool(thermostat_bits & THERM_HEAT),
        'thermostat_cool': bool(thermostat_bits & THERM_COOL),
    }

    # v0x07+: extended thermostat bits + timestamp
    if version >= 0x07:
        result['charge_allowed'] = bool(thermostat_bits & THERM_CHARGE_ALLOWED)
        result['charge_now'] = bool(thermostat_bits & THERM_CHARGE_NOW)
        result['sensor_fault'] = bool(thermostat_bits & THERM_SENSOR_FAULT)
        result['clamp_mismatch'] = bool(thermostat_bits & THERM_CLAMP_MISMATCH)
        result['interlock_fault'] = bool(thermostat_bits & THERM_INTERLOCK_FAULT)
        result['selftest_fail'] = bool(thermostat_bits & THERM_SELFTEST_FAIL)

        if len(raw_bytes) >= EVSE_V7_PAYLOAD_SIZE:
            device_ts = int.from_bytes(raw_bytes[8:12], 'little')
            result['device_timestamp'] = device_ts
            if device_ts > 0:
                result['device_time_unix'] = device_ts + SIDECHARGE_EPOCH
            else:
                result['device_time_unix'] = None  # not synced
                result['needs_time_sync'] = True

    return result
```

### 4.2 TIME_SYNC Trigger in lambda_handler()

After decoding the EVSE payload, check if the device needs a time sync:

```python
elif decoded.get('payload_type') == 'evse':
    # Store telemetry data (existing code)
    item['data'] = { ... }

    # v0x07: check if device needs TIME_SYNC
    if decoded.get('needs_time_sync'):
        print("Device needs TIME_SYNC (timestamp=0)")
        send_time_sync(ack_watermark=0)
```

### 4.3 DynamoDB Item Changes

The v0x07 decoded data adds these fields to the `data.evse` map in DynamoDB:

```python
'charge_allowed': True,
'charge_now': False,
'sensor_fault': False,
'clamp_mismatch': False,
'interlock_fault': False,
'selftest_fail': False,
'device_timestamp': 304697,       # SideCharge epoch
'device_time_unix': 2067530297,   # Unix epoch (for human readability)
```

Backward-compatible: v0x06 payloads simply don't include these fields.

---

## 5. Scheduler Lambda Changes (charge_scheduler_lambda.py)

### 5.1 TIME_SYNC on Daily Schedule

Add to `lambda_handler()`:

```python
def lambda_handler(event, context):
    # ... existing TOU/MOER logic ...

    # TIME_SYNC: send daily to correct drift
    last_state = get_last_state()
    last_sync_epoch = maybe_send_time_sync(last_state)
    # Store last_sync_epoch in the sentinel key:
    write_state(command, reason, moer_percent, tou_peak, last_sync_epoch)
```

### 5.2 ACK Watermark Lookup

The watermark is the device_timestamp of the most recent uplink the cloud successfully stored:

```python
def get_latest_uplink_watermark():
    """Query DynamoDB for the latest device_timestamp from this device."""
    resp = table.query(
        KeyConditionExpression=Key('device_id').eq(get_device_id()),
        ScanIndexForward=False,  # newest first
        Limit=5,
        FilterExpression=Attr('event_type').eq('evse_telemetry'),
    )
    for item in resp.get('Items', []):
        ts = item.get('data', {}).get('evse', {}).get('device_timestamp', 0)
        if ts and ts > 0:
            return int(ts)
    return 0
```

### 5.3 Delay Window Downlink (Future -- PDL-013)

The charge_scheduler currently sends the old 4-byte format:

```python
# Current: [0x10, allowed, 0x00, 0x00]
payload_bytes = bytes([CHARGE_CONTROL_CMD, 0x01 if allowed else 0x00, 0x00, 0x00])
```

The PRD specifies a new 10-byte delay window format. This is out of scope for TASK-033/034/035 but is documented here for reference:

```python
# Future (PDL-013):
# [0x10, 0x01, start_time(4B LE), end_time(4B LE)]  -- set delay window
# [0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]  -- clear delay

def send_delay_window(start_sc, end_sc):
    payload = bytes([0x10, 0x01]) + \
              start_sc.to_bytes(4, 'little') + \
              end_sc.to_bytes(4, 'little')
    send_sidewalk_msg(payload, transmit_mode=1)
```

**This does not block TASK-033/034/035.** The existing 4-byte charge control command continues to work alongside the new TIME_SYNC command.

---

## 6. Dependency Chain and Implementation Order

### 6.1 Confirmed Order: TASK-033 -> TASK-035 -> TASK-034

The PRD suggested 033 -> 034 -> 035. After analysis, I recommend a different order:

```
TASK-033 (TIME_SYNC)
    |
    v
TASK-035 (Uplink v0x07)  -- needs time_sync_now() from 033
    |
    v
TASK-034 (Event Buffer)  -- needs v0x07 entry format from 035
                          -- needs watermark trimming from 033
```

**Rationale**:

1. **TASK-033 first**: TIME_SYNC is the foundation. Without it, timestamps are 0 and the event buffer has nothing meaningful to trim. Also the smallest change: one new downlink handler, one new module (~80 lines of C), one Lambda function (~20 lines of Python).

2. **TASK-035 second**: Uplink v0x07 consumes `time_sync_now()` from TASK-033. The format change is self-contained: bump version, extend payload to 12 bytes, populate thermostat byte bits 2-7, append timestamp. Lambda gets a new decoder branch. Can be tested end-to-end: device sends v0x07 with timestamp, cloud decodes it, cloud responds with TIME_SYNC.

3. **TASK-034 last**: The event buffer depends on both: it stores `event_entry_t` (v0x07 format from TASK-035) and trims by watermark (from TASK-033). The buffer is the most complex change (new data structure, integration into the uplink path, trim logic) and benefits from having the other two pieces stable first.

### 6.2 Implementation Sizing

| Task | Device changes | Cloud changes | Estimated size |
|------|---------------|---------------|----------------|
| TASK-033 | `time_sync.c/h` (~80 LOC), `app_rx.c` handler (~15 LOC) | `decode_evse_lambda.py` sync trigger (~30 LOC), `charge_scheduler_lambda.py` daily sync (~20 LOC) | Small (S) |
| TASK-035 | `app_tx.c` format change (~20 LOC), `evse_payload.c` thermostat byte (~5 LOC) | `decode_evse_lambda.py` v7 decoder (~40 LOC) | Small (S) |
| TASK-034 | `event_buffer.c/h` (~120 LOC), `app_entry.c` integration (~15 LOC), `app_tx.c` buffer write (~10 LOC) | None (buffer is device-only; cloud already handles watermark via TIME_SYNC) | Medium (M) |

### 6.3 Branch Strategy

```
main
 └── feature/time-sync-downlink       (TASK-033)
      └── feature/uplink-v07          (TASK-035, branches from 033)
           └── feature/event-buffer   (TASK-034, branches from 035)
```

Each merges to main sequentially. Tests must pass at each stage.

---

## 7. Platform API Additions

### 7.1 No New Platform API Functions Required

Good news: the existing `platform_api.h` (version 3) already provides everything these three tasks need:

| Need | Existing API function | Notes |
|------|----------------------|-------|
| Current uptime | `uptime_ms()` | Used for time sync offset calculation |
| Send uplink | `send_msg()` | Already used; payload grows from 8 to 12 bytes |
| Receive downlink | `on_msg_received()` callback | Already dispatches to `app_rx.c` |
| Logging | `log_inf()`, `log_err()`, `log_wrn()` | Already available |
| Memory | `malloc()`, `free()` | Not needed -- event buffer is statically allocated |

**Platform API version stays at 3.** No platform image rebuild required. This is entirely an app-side change, which means it can be deployed via OTA.

### 7.2 What About Flash Persistence?

If we wanted to persist the event buffer across reboots, we would need:

```c
/* Hypothetical additions (NOT proposed for these tasks) */
int (*flash_read)(uint32_t addr, uint8_t *buf, size_t len);
int (*flash_write)(uint32_t addr, const uint8_t *buf, size_t len);
int (*flash_erase)(uint32_t addr, size_t len);
```

This would require:
- Bumping `PLATFORM_API_VERSION` to 4
- Platform image rebuild and reflash (not OTA-able)
- Careful flash wear management (nRF52840 NVMC: 10K erase cycles)
- Designating a flash region for app use (currently no partition allocated)

**Decision: Not needed for v1.0.** RAM-only buffer is sufficient. The event buffer is a convenience for gap-filling, not a critical data store. Reboot clears it, and the cloud handles the gap gracefully.

### 7.3 App Callback Table -- No Changes

The `app_callbacks` struct (version 3) does not need changes. The platform already calls `on_msg_received()` for downlinks (TIME_SYNC handled there) and `on_timer()` for periodic work (event buffer writes happen there).

---

## 8. New Files and Module Summary

### 8.1 New Device Files

| File | Module | Purpose |
|------|--------|---------|
| `include/time_sync.h` | Time sync | `time_sync_process()`, `time_sync_now()`, `time_sync_is_synced()` |
| `src/app_evse/time_sync.c` | Time sync | State management, clock computation |
| `include/event_buffer.h` | Event buffer | `event_buffer_write()`, `event_buffer_latest()`, `event_buffer_trim()`, `event_buffer_count()` |
| `src/app_evse/event_buffer.c` | Event buffer | Ring buffer implementation, static 600-byte allocation |

### 8.2 Modified Device Files

| File | Changes |
|------|---------|
| `src/app_evse/app_rx.c` | Add 0x30 TIME_SYNC handler |
| `src/app_evse/app_tx.c` | Bump to v0x07 (12B payload), add event buffer write |
| `src/app_evse/app_entry.c` | Init time_sync and event_buffer modules, set_api calls |
| `src/app_evse/evse_payload.c` | OR charge_control bit into thermostat_flags |
| `include/evse_payload.h` | Payload struct + function declarations |
| `app_evse/CMakeLists.txt` | Add time_sync.c, event_buffer.c to sources |

### 8.3 Modified Cloud Files

| File | Changes |
|------|---------|
| `aws/decode_evse_lambda.py` | v0x07 decoder, thermostat bit parsing, TIME_SYNC trigger on timestamp=0 |
| `aws/charge_scheduler_lambda.py` | Daily TIME_SYNC with ACK watermark, watermark lookup |
| `aws/sidewalk_utils.py` | Add `send_time_sync()` helper |

### 8.4 New Tests

| File | Tests |
|------|-------|
| `tests/test_app.c` | `test_time_sync_*` (process, now computation, not-synced returns 0, drift), `test_event_buffer_*` (write, read latest, trim, overflow wrap, trim with watermark 0) |
| `aws/tests/test_decode_evse_lambda.py` | v0x07 decoding, thermostat bits, timestamp=0 sync trigger, backward compat with v0x06 |

---

## 9. RAM Budget Impact

| Component | Current | After TASK-033/034/035 |
|-----------|---------|----------------------|
| time_sync state | 0 | 13 bytes |
| event buffer | 0 | 606 bytes |
| Existing app statics | ~800 bytes (est.) | ~800 bytes |
| Stack | ~2000 bytes (est.) | ~2000 bytes |
| **Total estimated** | **~2800 bytes** | **~3419 bytes** |
| **Available** | **8192 bytes** | **8192 bytes** |
| **Headroom** | **~5392 bytes** | **~4773 bytes** |

The event buffer is the single largest new allocation (606 bytes, 7.4% of budget). After all three tasks, estimated headroom is ~4.7KB -- plenty for future features.

---

## 10. Sequence Diagram: End-to-End Flow

```
Device (app)                    Cloud (Lambdas)
    |                               |
    |--- Uplink v0x07 (ts=0) ------>|  decode Lambda sees ts=0
    |                               |  => send_time_sync(wm=0)
    |<-- TIME_SYNC (time, wm=0) ----|
    |                               |
    |  time_sync_process():         |
    |    sync_time = cloud_time     |
    |    sync_uptime = uptime_ms()  |
    |    synced = true              |
    |                               |
    |  [15 min later: heartbeat]    |
    |                               |
    |  Build event_entry_t:         |
    |    sensors + time_sync_now()  |
    |  event_buffer_write(entry)    |
    |                               |
    |--- Uplink v0x07 (ts=304697)->|  decode Lambda stores telemetry
    |                               |    device_timestamp: 304697
    |                               |
    |  [Later: scheduler runs]      |
    |                               |  get_latest_uplink_watermark() => 304697
    |<-- TIME_SYNC (time, wm=304697)|
    |                               |
    |  time_sync_process():         |
    |    update sync_time           |
    |    event_buffer_trim(304697)  |
    |    => entries <= 304697 freed  |
    |                               |
```

---

## 11. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| TIME_SYNC downlink lost (LoRa drop) | Medium | Device stays unsynced, sends ts=0, cloud retries on next uplink | Self-healing: every uplink with ts=0 triggers a sync attempt |
| ACK watermark lost | Medium | Buffer doesn't trim, eventually wraps and overwrites oldest | Acceptable: ring buffer wrap is the designed overflow behavior |
| Clock drift exceeds 5 min | Very Low | Would require >35 days without re-sync (100 ppm) | Daily re-sync from scheduler Lambda |
| Event buffer fills before ACK | Low | Oldest entries overwritten | By design: 50 entries = 12.5 hours at 15-min heartbeat; ACK arrives on next downlink opportunity |
| decode Lambda fails to trigger TIME_SYNC | Low | Device stays unsynced until daily scheduler sync | Two independent sync paths: decode Lambda (reactive) + scheduler (proactive) |
| v0x07 decode breaks v0x06 | None | Version byte dispatches to different code paths | Explicit version check; v0x06 path unchanged |

---

## 12. Open Questions

1. **Should the event buffer support replaying old entries?** Current design sends only the latest state. If we wanted the cloud to request specific historical entries (e.g., "send me entries 5-10"), that would require a new downlink command and a more complex uplink protocol. **Recommendation: Not for v1.0.** The heartbeat + gap tolerance approach is sufficient.

2. **Should TIME_SYNC include a sequence number?** A monotonically increasing seq would let the device detect replayed or out-of-order TIME_SYNC commands. **Recommendation: Not needed.** TIME_SYNC is idempotent (the device always takes the most recent time), and the SideCharge epoch only moves forward. A stale TIME_SYNC would set the clock slightly behind, corrected on the next sync.

3. **Should the scheduler piggyback the ACK watermark on charge control downlinks too?** This would increase ACK frequency and reduce buffer retention time. **Recommendation: Yes, eventually, but not in this iteration.** The charge control command format (0x10) is also changing (PDL-013 delay windows). Wait until that redesign to add the watermark field.
