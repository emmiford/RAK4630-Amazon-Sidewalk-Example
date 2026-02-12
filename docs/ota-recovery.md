# OTA Recovery and Rollback Runbook

TASK-008 | Author: Eero (testing architect) | 2026-02-12

This runbook is for operators diagnosing and recovering from OTA failures on RAK4631 Sidewalk devices. It assumes you have serial console access or SSH to a machine with pyOCD connected to the device.

---

## 1. OTA State Machine

### Phase Diagram

```
                       START msg
  IDLE ──────────────────────────────► RECEIVING
   ▲                                      │
   │  abort / error                       │ all chunks received
   │                                      ▼
   │                                  VALIDATING
   │                                      │
   │  CRC mismatch ──► ERROR              │ CRC OK
   │                     │                ▼
   │                     │            COMPLETE ── 15s delay ──► APPLYING ──► reboot
   │                     │                                        │
   └─────────────────────┘                                        │
                                                                  │
                                           power loss here ──► recovery on next boot
```

### Phase Descriptions

| Phase | Description | Duration |
|-------|-------------|----------|
| **IDLE** | No OTA in progress. Waiting for START message from cloud. | Indefinite |
| **RECEIVING** | Firmware chunks arriving via Sidewalk LoRa downlinks. Each chunk is 15 bytes of payload (19 bytes total with 4-byte header). Sequential in full mode, sparse in delta mode. | Minutes to hours depending on image size and LoRa conditions |
| **VALIDATING** | All chunks received. CRC32 computed over staged image (or merged staging+primary for delta). | < 1 second |
| **COMPLETE** | CRC validated. COMPLETE uplink sent to cloud. Waiting for 15-second deferred apply timer. | Exactly 15 seconds |
| **APPLYING** | Copying staged image from staging area (0xD0000) to app primary (0x90000), page by page (4KB pages). Recovery metadata updated after each page. | < 5 seconds for typical app images |
| **ERROR** | Something failed (CRC mismatch, flash error, magic verification failure). Returns to IDLE on next START or abort. | Until cleared |

### The 15-Second Apply Delay

After validation succeeds, the device enters COMPLETE phase and schedules the apply 15 seconds later. This delay exists so the COMPLETE uplink message has time to transmit via LoRa before the device reboots. Without this delay, the cloud would never learn the OTA succeeded.

During this 15-second window:
- The device is still running normally
- An ABORT message can still cancel the apply
- Power loss here means the validated image sits in staging; it will not be applied automatically on next boot (no APPLYING metadata was written yet)

---

## 2. Recovery Metadata

### Flash Location

Recovery metadata is stored at **0xCFF00** (256 bytes), between the app primary partition and the OTA staging area.

### Struct Layout

```c
struct ota_metadata {
    uint32_t magic;          /* 0x4F544155 = "OTAU" */
    uint8_t  state;          /* 0x00=NONE, 0x01=STAGED, 0x02=APPLYING */
    uint8_t  reserved[3];
    uint32_t image_size;     /* Size of staged image in bytes */
    uint32_t image_crc32;    /* Expected CRC32 of staged image */
    uint32_t app_version;    /* Version from OTA_START message */
    uint32_t pages_copied;   /* Number of 4KB pages already copied */
    uint32_t total_pages;    /* Total 4KB pages to copy */
};
```

Total struct size: 28 bytes. Fits in a single flash write.

### What Survives Power Loss

The metadata is written to its own flash page (0xCFF00). It is updated after each page copy during the APPLYING phase. On power loss during apply:

- `magic` = 0x4F544155 — confirms this is valid OTA metadata
- `state` = 0x02 (APPLYING) — tells boot recovery to resume
- `pages_copied` = last successfully completed page — resume point
- `total_pages` = total pages needed — completion target
- `image_size`, `image_crc32`, `app_version` — preserved for the resume operation

On next boot, `ota_boot_recovery_check()` reads this metadata. If state is APPLYING, it calls `ota_resume_apply()` starting from `pages_copied`.

### State Meanings

| State | Value | Boot Behavior |
|-------|-------|---------------|
| NONE | 0x00 | Normal boot, no action |
| STAGED | 0x01 | Image staged but never applied. Metadata cleared, normal boot. |
| APPLYING | 0x02 | Interrupted apply. Recovery resumes from `pages_copied`. |

---

## 3. Failure Modes

### 3a. Power Loss During RECEIVING

**What happens**: OTA session state is in RAM only during RECEIVING. All progress is lost.

**Result**: Device boots normally. Cloud-side session may still be active in DynamoDB.

**Recovery**: Cloud resends the OTA from the beginning. The device will erase staging and start fresh when it receives a new START message. Use `ota_deploy.py abort` to clear the stale cloud session first, then redeploy.

### 3b. CRC Mismatch After VALIDATING

**What happens**: All chunks were received, but the CRC32 computed over the staged image does not match the expected CRC32 from the START message.

**Log output**:
```
OTA: CRC32 mismatch (calc=0x1A2B3C4D, expected=0x5E6F7A8B)
```

**Result**: Device sends COMPLETE uplink with status `OTA_STATUS_CRC_ERR` (1). Phase moves to ERROR. The bad image remains in staging but is never applied.

**Cause**: Corrupted chunk during LoRa transmission (rare — AEAD should catch this), or firmware binary mismatch between what the cloud uploaded and what was chunked.

**Recovery**: Automatic return to IDLE on next START message. Resend the OTA.

### 3c. Power Loss During APPLYING

**What happens**: The page-by-page copy from staging (0xD0000) to primary (0x90000) was interrupted. Some pages are from the new image, some are from the old image (or erased).

**Result**: App primary partition is in an inconsistent state. The app will not load.

**Recovery**: **Automatic.** On next boot, `ota_boot_recovery_check()` detects APPLYING metadata and resumes the copy from the last completed page. After completing all pages, it verifies the APP_CALLBACK_MAGIC (0x53415050) at the start of the primary partition, clears metadata, and reboots.

**Log output on recovery boot**:
```
OTA: detected interrupted apply, resuming...
OTA: resuming interrupted apply (page 3/5)
OTA recovery: complete, clearing metadata and rebooting
```

### 3d. Magic Verification Failure After Apply

**What happens**: All pages were copied from staging to primary, but the first 4 bytes of the primary partition do not match APP_CALLBACK_MAGIC (0x53415050 = "SAPP").

**Log output**:
```
OTA: magic check failed after apply (got 0xDEADBEEF)
```
or during recovery:
```
OTA recovery: magic check failed (got 0x00000000)
```

**Result**: The copy completed but the image is corrupted or not a valid app image. The device does NOT reboot. OTA phase moves to ERROR. The device boots in platform-only mode — Sidewalk still works, OTA engine still works, but no app callbacks fire.

**Recovery**: Send a new OTA with a valid app image, or physically reflash via `flash.sh app`.

### 3e. Stale Flash Data (KI-003)

**What happens**: The app primary partition contains stale data from a previous larger image beyond the current image's size. When `ota_deploy.py baseline` reads the partition, it includes this stale data, inflating the baseline.

**Result**: Delta OTA computes against an inflated baseline. More chunks are marked as "changed" than actually differ. The OTA still works correctly but transfers more data than necessary.

**Log clue**: `ota_deploy.py baseline` reports a baseline significantly larger than the app binary (e.g., 4524 bytes vs 239 bytes).

**Workaround**: Reflash the app via `flash.sh app` before capturing a new baseline. See KI-003 in `docs/known-issues.md` for the planned fix.

---

## 4. Diagnosis

### Shell Commands

Connect to the device serial console:
```bash
screen /dev/tty.usbmodem101 115200
```

#### `sid ota status`

Reports the current OTA phase and progress.

| Output | Meaning |
|--------|---------|
| `IDLE` | No OTA in progress |
| `RECEIVING 15/100` | Receiving chunks, 15 of 100 received |
| `VALIDATING` | Computing CRC over staged image |
| `COMPLETE` | CRC OK, waiting for 15s apply delay |
| `APPLYING` | Copying staging to primary (do NOT power off) |
| `ERROR` | Something failed, check logs |

#### `sid status`

Reports overall device state including whether the app image loaded.

| Output | Meaning |
|--------|---------|
| `App image: LOADED (version 5)` | App is running normally |
| `App image: NOT LOADED (bad magic)` | Primary partition has no valid app — corrupted or erased |
| `App image: NOT LOADED (version mismatch)` | Platform and app API versions differ (KI-001) |

### Key Log Messages

| Log Message | What It Means |
|-------------|---------------|
| `OTA START: size=4096 chunks=274/274 chunk_size=15 crc=0xABCD1234 ver=6` | New OTA session starting. Full mode (chunks match full image). |
| `OTA START: size=4096 chunks=12/274 ... DELTA` | Delta mode — only 12 of 274 chunks will be sent. |
| `OTA START: firmware already applied (CRC 0xABCD1234)` | Device already has this firmware. Cloud retransmitted START after a reboot. No action needed. |
| `OTA CHUNK 15/100: 15 bytes at 0xD00E1` | Chunk received and written to staging. Normal progress. |
| `OTA: CRC32 OK (0xABCD1234), scheduling apply in 15s` | Validation passed. Apply will fire in 15 seconds. |
| `OTA: deferred apply firing after 15s delay` | Apply is starting now. Do not power off. |
| `OTA: applying update (size=4096, crc=0xABCD1234)` | Page-by-page copy in progress. |
| `OTA: apply complete, clearing metadata and rebooting` | Success. Device will reboot momentarily. |
| `OTA: detected interrupted apply, resuming...` | Boot recovery kicked in. Power was lost during previous apply. |
| `OTA: abort received` | Cloud sent an ABORT. Session cleared. |
| `OTA START: busy (phase=APPLYING), rejecting` | Device is mid-apply and cannot accept a new OTA. |

---

## 5. Manual Recovery Procedures

### 5a. OTA Stuck in RECEIVING

**Symptom**: `sid ota status` shows RECEIVING but progress has stalled. Cloud-side status shows retries climbing.

**Diagnosis**: Check `ota_deploy.py status` — if `last activity` is > 5 minutes, the session is stalled.

**Fix**:
```bash
# From your workstation:
python3 aws/ota_deploy.py abort        # Sends OTA_ABORT to device + clears DynamoDB session
python3 aws/ota_deploy.py deploy --build --version <N>   # Redeploy
```

If the device is unreachable (no Sidewalk connectivity), the ABORT will not be delivered. Wait for the device to reconnect, or power-cycle it (RECEIVING state is RAM-only, lost on reboot).

### 5b. OTA Failed Validation (CRC Mismatch)

**Symptom**: Cloud shows status "error" or "crc_err". Device log shows CRC mismatch.

**Diagnosis**: The image in staging is corrupted. This is rare over AEAD-protected LoRa.

**Fix**: No device-side action needed. The device moves to ERROR phase and accepts a new START message.
```bash
python3 aws/ota_deploy.py abort          # Clear the failed session
python3 aws/ota_deploy.py deploy --build  # Rebuild and resend
```

### 5c. OTA Applied but App Will Not Load

**Symptom**: Device rebooted after OTA. `sid status` shows `App image: NOT LOADED (version mismatch)`.

**Diagnosis**: The OTA'd app was built against a different `APP_CALLBACK_VERSION` than the platform expects. This is KI-001.

**Fix**: OTA the correct app version (one built against the same platform API version):
```bash
# Ensure the app source has the correct APP_CALLBACK_VERSION
python3 aws/ota_deploy.py deploy --build --version <N>
```

The device is in platform-only mode but the OTA engine still works. You can push a corrected app over the air.

### 5d. Device Completely Unresponsive

**Symptom**: No serial console output. No Sidewalk connectivity. No response to any commands.

**Diagnosis**: Possible causes:
- Power supply issue
- Platform image corruption (requires physical reflash)
- Hardware failure

**Fix**: Physical reflash via pyOCD programmer:
```bash
cd app/rak4631_evse_monitor
./flash.sh all    # Flash MFG + platform + app
```

If only the app is suspected:
```bash
./flash.sh app    # Flash app only, platform untouched
```

After reflashing, recapture the baseline:
```bash
python3 aws/ota_deploy.py baseline
```

### 5e. Power Loss During Apply

**Symptom**: Device rebooted unexpectedly during OTA apply. On next boot, you see recovery log messages.

**Diagnosis**: `ota_boot_recovery_check()` detected APPLYING metadata and is resuming.

**Fix**: **No manual action needed.** The recovery is automatic:
1. Boot detects metadata with state=APPLYING
2. Resumes page copy from `pages_copied`
3. Verifies APP_CALLBACK_MAGIC after copy
4. Clears metadata
5. Reboots into the new app

If recovery itself is interrupted by another power loss, it will resume again on the next boot. The metadata tracks progress, so it converges.

Monitor the serial console to confirm:
```
OTA: detected interrupted apply, resuming...
OTA: resuming interrupted apply (page 3/5)
OTA recovery: complete, clearing metadata and rebooting
```

---

## 6. Cloud-Side Operations

All cloud operations use `aws/ota_deploy.py`. Requires AWS credentials and (for some operations) a pyOCD connection to the device.

### Check OTA Status

```bash
# One-shot status check
python3 aws/ota_deploy.py status

# Live monitoring (poll every 30 seconds)
python3 aws/ota_deploy.py status --watch

# Custom poll interval (every 10 seconds)
python3 aws/ota_deploy.py status --watch 10
```

Output shows: firmware version, progress bar, chunks sent/total, elapsed time, ETA, baseline info, and transfer mode (full/delta).

### Abort an OTA

```bash
# Send OTA_ABORT to device AND clear DynamoDB session
python3 aws/ota_deploy.py abort

# Clear DynamoDB session only (device not reachable)
python3 aws/ota_deploy.py clear-session
```

`abort` sends a downlink ABORT message (cmd=0x20, sub=0x03) via IoT Wireless, then clears the session. Use `clear-session` if the device is offline and you just need to reset the cloud state.

### Deploy (Full Mode)

```bash
# Build, upload, and monitor
python3 aws/ota_deploy.py deploy --build --version 6

# Deploy pre-built binary (no build step)
python3 aws/ota_deploy.py deploy

# Deploy without pyOCD verification (device not physically connected)
python3 aws/ota_deploy.py deploy --build --remote

# Force deploy over an existing session
python3 aws/ota_deploy.py deploy --build --force
```

The deploy command:
1. Optionally patches `EVSE_VERSION` and rebuilds
2. Downloads S3 baseline and verifies it matches device primary (unless `--remote`)
3. Computes delta, shows preview
4. Uploads firmware to S3 (triggers Lambda)
5. Monitors progress until COMPLETE

If delta shows 0 changed chunks, the firmware matches the baseline and nothing is deployed.

### Deploy (Delta Mode)

Delta mode is automatic. When the deploy tool detects that fewer chunks differ than the full image, it uploads the full binary but the Lambda sends only changed chunks. The device receives a START message with `total_chunks < full_image_chunks`, which triggers delta mode on-device.

Delta mode requires a valid baseline:
```bash
# Capture baseline first (requires pyOCD)
python3 aws/ota_deploy.py baseline

# Then deploy (delta computed automatically)
python3 aws/ota_deploy.py deploy --build
```

### Capture Baseline

```bash
python3 aws/ota_deploy.py baseline
```

Reads the device's app primary partition (256KB at 0x90000) via pyOCD, trims trailing 0xFF, computes CRC32, and uploads to S3. Also saves a local copy at `/tmp/ota_baseline.bin`.

### Preview Delta (Dry Run)

```bash
python3 aws/ota_deploy.py preview
```

Shows which chunks differ between the local build and the S3 baseline, with byte-level hex diffs. Does not deploy anything. Useful for verifying that only expected changes will be sent.

---

## 7. Rollback Limitations

### No Automatic Rollback

This OTA system does **not** support automatic rollback to a previous app version. There is:

- **No dual-bank A/B partition scheme.** There is one app primary partition (0x90000, 256KB) and one staging area (0xD0000, 148KB). The staging area is a receive buffer, not a backup.
- **No "known good" image preserved.** Once the apply copies staging to primary, the old app is overwritten.
- **No boot counter or watchdog rollback.** If the new app crashes, the device stays on the new (broken) app.

### What Recovery Actually Does

The recovery mechanism (`ota_boot_recovery_check`) only handles one scenario: **power loss during the APPLYING phase**. It resumes the interrupted page-by-page copy. It does not:

- Revert to the previous app version
- Detect a crashing app and switch back
- Maintain any history of previous firmware versions

### How to Rollback Manually

To revert to a previous app version, you must deploy it as a new OTA:

```bash
# Option 1: OTA deploy the old version (if device has Sidewalk connectivity)
git checkout <old-commit>
python3 aws/ota_deploy.py deploy --build --version <old-version>

# Option 2: Physical reflash (if device is unreachable)
cd app/rak4631_evse_monitor
./flash.sh app
```

After rollback, recapture the baseline:
```bash
python3 aws/ota_deploy.py baseline
```

---

## 8. Prevention

### Always Capture Baseline After Successful OTA

After every successful OTA, recapture the baseline so that the next delta OTA computes correctly:
```bash
python3 aws/ota_deploy.py baseline
```

Without a fresh baseline, delta mode will either send more chunks than necessary (KI-003) or fail to detect actual changes.

### Use Delta Mode to Minimize Transfer Time

Delta mode only sends chunks that differ between the baseline and the new firmware. For small code changes, this can reduce transfer from hundreds of chunks to a handful.

Each LoRa chunk takes approximately 15 seconds (downlink scheduling + ACK). Reducing from 274 chunks (full 4KB image) to 12 chunks (small delta) cuts OTA time from ~70 minutes to ~3 minutes.

```bash
# Preview delta before deploying
python3 aws/ota_deploy.py preview
```

### Monitor OTA Status

During an OTA, watch for stalls:
```bash
python3 aws/ota_deploy.py status --watch 10
```

Signs of trouble:
- `last activity` growing beyond 2-3 minutes with no chunk progress
- `retries` climbing rapidly (device not ACKing)
- Status stuck on `sending_chunk` for > 5 minutes

If stalled, abort and retry:
```bash
python3 aws/ota_deploy.py abort
python3 aws/ota_deploy.py deploy --build
```

### Do Not Power Off During APPLYING

The APPLYING phase is the only phase where power loss can leave the device in a partially-written state. The automatic recovery handles this, but it adds an extra boot cycle and relies on staging data being intact.

The APPLYING phase is fast (< 5 seconds for typical app images), but if you see this log message, do not disconnect power:
```
OTA: deferred apply firing after 15s delay
```

Wait until you see:
```
OTA: apply complete, clearing metadata and rebooting
```

---

## Quick Reference: Decision Tree

```
Device not sending telemetry after OTA?
├── sid status → "NOT LOADED (version mismatch)"
│   └── KI-001: OTA the correct app version
├── sid status → "NOT LOADED (bad magic)"
│   └── Corrupted apply. Resend OTA or flash.sh app
├── sid status → "LOADED" but no uplinks
│   └── App bug. Check app logs, rollback via OTA
└── No serial output at all
    └── flash.sh all (full reflash)

OTA not progressing?
├── sid ota status → RECEIVING, chunks stuck
│   └── ota_deploy.py abort + redeploy
├── sid ota status → ERROR
│   └── Check logs for CRC/flash error. abort + redeploy
├── sid ota status → IDLE (cloud thinks it's active)
│   └── ota_deploy.py clear-session + redeploy
└── sid ota status → APPLYING
    └── DO NOT POWER OFF. Wait for reboot or recovery.
```

---

## Flash Address Quick Reference

| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| Platform API | 0x8FF00 | 256B | Platform function pointer table |
| App Primary | 0x90000 | 256KB | Running app image |
| OTA Metadata | 0xCFF00 | 256B | Recovery state (survives power loss) |
| OTA Staging | 0xD0000 | 148KB | Incoming firmware buffer |

## Test Coverage

The OTA recovery path is covered by 16 host-side unit tests in `tests/app/test_ota_recovery.c`:

- **Normal boot (4 tests)**: No metadata, bad magic, STAGED state, NONE state — all confirm no recovery triggered.
- **Recovery path (4 tests)**: Full apply from scratch, resume at page 3 of 5, partial last page, already-complete apply — all confirm correct page copy and reboot.
- **Magic verification (1 test)**: Staging image without APP_CALLBACK_MAGIC — confirms recovery attempt but no reboot.
- **Message processing (6 tests)**: START, ABORT, unknown subtype, short messages, wrong cmd type — confirms state machine transitions.
- **Helpers (1 test)**: Phase string conversion.
