# Device Provisioning Guide

How to provision a new RAK4631 EVSE monitor for Amazon Sidewalk.

## Overview

Provisioning is a one-time factory process. Once complete, the device operates over LoRa indefinitely with no further BLE or physical access required.

```
Factory (one-time)                    Field (forever)
─────────────────                     ───────────────
1. Generate credentials (AWS)
2. Build mfg.hex (Nordic tools)
3. Flash: mfg + platform + app
4. First boot: BLE registration ──→  LoRa data link (no BLE needed)
5. Reboot: LoRa comes up
6. Back up sidewalk keys
```

## Prerequisites

- RAK4631 board (nRF52840 + SX1262) connected via USB
- pyOCD installed: `pip install pyocd`
- AWS CLI configured with appropriate permissions
- Access to Amazon Sidewalk console for credential generation
- nRF Connect SDK v2.9.1 (for firmware builds only)
- An Amazon Echo or Sidewalk-enabled gateway within BLE range (~10m)

## Step 1: AWS IoT Wireless — Create Device

Register the device in the Sidewalk console:

1. Go to **AWS IoT Core → Sidewalk → Provision device**
2. Create a device profile (or reuse existing)
3. Download two credential files:
   - `device_profile.json` — contains DeviceTypeId, ApplicationServerPublicKey
   - `wireless_device.json` — contains device key pairs (SMSN, APID, ed25519 keys)

See `credentials.example/` for the expected format. The wireless device file contains:

```json
{
  "smsn": "...",
  "app_srv_pub_key": "...",
  "device_priv_key": "...",
  "device_pub_key": "...",
  "apid": "...",
  "ed25519_device_priv_key": "...",
  "ed25519_device_pub_key": "..."
}
```

Store credentials in `~/.sidewalk/credentials/<device-id>/`.

### Register via CLI (alternative)

If not using the console provisioning flow:

```bash
aws iotwireless create-wireless-device \
  --type Sidewalk \
  --name "evse-monitor-01" \
  --destination-name sidewalk-evse-destination \
  --sidewalk '{"SidewalkManufacturingSn": "<SMSN>"}'
```

Note the returned `Id` (UUID) — this is used by `ota_deploy.py` and `sidewalk_utils.py`. The device ID is currently hardcoded in `aws/ota_deploy.py`. For additional devices, update this or use the auto-discovery in `sidewalk_utils.py`.

## Step 2: Generate MFG Page

Use the Nordic Sidewalk tools to convert credentials into a flashable manufacturing page:

```shell
# From the nRF Connect SDK sidewalk tools
python3 tools/provision/provision.py \
  --config ~/.sidewalk/credentials/<device-id>/device_profile.json \
  --wireless_device ~/.sidewalk/credentials/<device-id>/wireless_device.json \
  --output mfg.hex
```

This produces `mfg.hex` — a 4KB image for flash address 0xFF000 containing the device's unique identity and crypto keys.

## Step 3: Flash Everything

Flash order matters. MFG must go first (platform flash erases HUK).

```shell
# 1. MFG credentials (4KB @ 0xFF000)
pyocd flash --target nrf52840 mfg.hex

# 2. Platform firmware (576KB @ 0x00000)
pyocd flash --target nrf52840 build/rak4631_evse_monitor/zephyr/zephyr.hex

# 3. App firmware (4KB @ 0x90000)
pyocd flash --target nrf52840 build_app/app.hex
```

Or use the flash script:
```shell
bash app/rak4631_evse_monitor/flash.sh all
```

**Safety warnings:**
- **Never chip erase** without backing up Sidewalk session keys:
  ```bash
  pyocd cmd -c "savemem 0xF8000 0x7000 sidewalk_keys.bin"
  ```
- **Platform flash erases HUK** — PSA crypto keys must be re-derived. Always flash MFG first, then platform, then app.

## Step 4: First Boot — BLE Registration

On first power-up, the Sidewalk SDK:

1. Detects no session keys in flash
2. Enables BLE advertising
3. Connects to the nearest Amazon Sidewalk gateway (Echo device)
4. Negotiates session keys via the cloud
5. Writes session keys to the `sidewalk` partition (0xF8000)

Connect the serial console (use `cu.` port, not `tty.` — the tty variant toggles DTR which resets the device):

```bash
screen /dev/cu.usbmodem101 115200
```

You'll see on the serial console:
```
Device Is registered, Time Sync Success, Link status: {BLE: Down, FSK: Down, LoRa: Down}
BT Connected
BT Disconnected Reason: 0x16 = LOCALHOST_TERM_CONN
```

**BLE is only needed for this initial registration.** After session keys are stored, the device never needs BLE again — all subsequent boots go straight to LoRa.

### Requirements for BLE registration
- An Amazon Echo or Sidewalk-enabled gateway within BLE range (~10m)
- The gateway must be on the same AWS account as the device registration
- Takes 5-15 seconds

## Step 5: Reboot for LoRa

The first boot does BLE registration but LoRa does NOT come up. This is a known Sidewalk SDK behavior — LoRa session initialization only runs on a boot where session keys are already present.

```shell
# Via serial console:
kernel reboot cold

# Or via programmer:
pyocd reset --target nrf52840
```

After reboot you'll see:
```
Device Is registered, Time Sync Success, Link status: {BLE: Down, FSK: Down, LoRa: Down}
Sidewalk READY
```

The device is now operational on LoRa.

## Step 6: Back Up Sidewalk Keys

**Critical.** The session keys at 0xF8000 cannot be regenerated without repeating BLE registration. Back them up immediately:

```shell
pyocd cmd -c "savemem 0xF8000 0x7000 sidewalk_keys_<device-id>.bin" --target nrf52840
```

Store this backup securely. If the sidewalk partition is ever lost (chip erase, flash corruption), you can restore it:

```shell
pyocd flash --target nrf52840 --base-address 0xF8000 sidewalk_keys_<device-id>.bin
```

## Step 7: Capture OTA Baseline

After verifying the device works, capture the current firmware as the OTA baseline for future delta updates:

```bash
python3 aws/ota_deploy.py baseline
```

This reads the device's app partition and uploads it to S3 as the reference for delta OTA. Without a baseline, OTA falls back to full mode (~69 min vs seconds for delta).

## Verification

### Device side (serial console, 115200 baud)
```
sid status        # Ready: YES, Link type: LoRa (0x4), App image: LOADED
sid mfg           # MFG version, device ID, key presence OK
app evse status   # J1772 state, voltage, current readings
```

If `sid mfg` shows keys as MISSING, the MFG partition was not flashed correctly or HUK was erased. Re-flash MFG and platform.

### Cloud side
```shell
# Check for uplinks in DynamoDB
aws dynamodb query \
  --table-name evse-events \
  --key-condition-expression "device_id = :d" \
  --expression-attribute-values '{":d": {"S": "<device-id>"}}'  \
  --scan-index-forward --limit 5
```

Verify the item contains `pilot_state`, `pilot_voltage_mv`, `current_ma`, and `thermostat_flags`.

## When BLE Is Needed Again

BLE registration is only required if session keys are lost. This happens when:

| Scenario | Cause | Prevention |
|----------|-------|------------|
| `sid reset` shell command | Explicitly wipes session keys | Don't run in production |
| Chip erase | Destroys entire flash including sidewalk partition | Never chip erase — use sector-level flash instead |
| HUK loss | PSA-encrypted keys become unreadable | Always flash MFG before platform (restores HUK) |

**In normal operation — including platform reflash, app OTA updates, and reboots — BLE is never needed.** The session keys persist in flash and are not affected by any update path.

### If re-registration is needed
1. Ensure an Echo gateway is within BLE range
2. Reboot the device — it will auto-detect missing keys and start BLE advertising
3. Registration completes automatically (5-15s)
4. Reboot again for LoRa
5. Back up the new session keys

## LoRa-Only Mode (Optional)

If you want to disable BLE entirely after provisioning to save power and reduce attack surface:

```
# On serial console — switch to LoRa only
sid lora
```

This sets `link_mask = SID_LINK_TYPE_3` (LoRa only). Note: if session keys are ever lost, you'll need to temporarily re-enable BLE (`sid ble`) to re-register.

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Unregistered` after flash | MFG page missing or corrupt | Reflash mfg.hex |
| `Time Sync Fail` | No session keys (first boot) | Wait for BLE registration, then reboot |
| LoRa stays Down after reboot | Session keys not yet written | Check BLE registration completed, reboot again |
| `sid mfg` shows keys MISSING | MFG partition erased or HUK changed | Re-flash MFG, then platform, then app |
| No data in DynamoDB | IoT Rule not configured | Check `aws/terraform/` is applied |
| `PSA -149 INVALID_SIGNATURE` | Normal Sidewalk background noise | Ignore — not an error |
| USB serial not appearing | VBUS not connected on RAK4631 USB port | Check USB cable and `USBREGSTATUS` register |
