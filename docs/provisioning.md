# Device Provisioning Guide

How to set up a new RAK4631 device for the EVSE Sidewalk monitor.

## Prerequisites

- RAK4631 board (nRF52840 + SX1262) connected via USB
- pyOCD installed: `pip install pyocd`
- AWS CLI configured with appropriate permissions
- Access to Amazon Sidewalk console for credential generation
- nRF Connect SDK v2.9.1 (for firmware builds only)

## 1. Generate Sidewalk Credentials

### Via AWS IoT Wireless Console

1. Go to **AWS IoT > Sidewalk > Provision device**
2. Create a new device profile (or reuse existing)
3. Download the credential files:
   - `device_profile.json` — device type and application server public key
   - `wireless_device.json` — device keys (ED25519 + P256R1 key pairs, SMSN, APID)

### Credential File Format

See `credentials.example/` for templates. The wireless device file contains:

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

Place credentials in `~/.sidewalk/credentials/<device-id>/`.

## 2. Build the MFG Partition

The MFG partition (4KB at `0xFF000`) stores device credentials in binary format. Generate `mfg.hex` using the Nordic Sidewalk credential tools:

```bash
# Use the Nordic Sidewalk provisioning script (part of NCS)
python3 tools/provision.py \
  --device-profile device_profile.json \
  --wireless-device wireless_device.json \
  --output mfg.hex
```

The resulting `mfg.hex` is device-specific and should not be shared.

## 3. Flash the Device

### Full Flash (new device)

Flash all three partitions in order — MFG first, then platform, then app:

```bash
bash app/rak4631_evse_monitor/flash.sh all
```

This flashes:
1. `mfg.hex` → `0xFF000` (4KB credentials)
2. Platform image → `0x00000` (576KB Zephyr + Sidewalk stack)
3. App image → `0x90000` (4KB EVSE logic)

### MFG Only (re-provisioning)

```bash
bash app/rak4631_evse_monitor/flash.sh mfg
```

Or directly:

```bash
pyocd flash --target nrf52840 mfg.hex
```

### Safety Warnings

- **Never chip erase** without backing up Sidewalk session keys:
  ```bash
  pyocd cmd -c "savemem 0xF8000 0x7000 sidewalk_keys.bin"
  ```
- **Platform flash erases HUK** — PSA crypto keys must be re-derived. Always flash MFG first, then platform, then app.
- **Reboot after full flash** — first boot does BLE registration, LoRa won't connect until second boot.

## 4. First Boot and Registration

1. Connect serial console:
   ```bash
   screen /dev/cu.usbmodem101 115200
   ```
   (Use `cu.usbmodem*`, not `tty.usbmodem*` — the tty variant toggles DTR which resets the device.)

2. Power the device. Watch for boot logs:
   ```
   Sidewalk READY
   Link status: {BLE: Up, FSK: Down, LoRa: Up}
   ```

3. First boot registers the device via BLE with the nearest Sidewalk gateway. This takes 30–60 seconds.

4. **Reboot the device** (button press or power cycle). LoRa link comes up on second boot.

## 5. Verify on Device

```
sid status        # Should show: Ready: YES, LoRa: Up
sid mfg           # Should show: MFG version, device ID, key presence OK
app evse status   # Should show: J1772 state, voltage, current readings
```

If `sid mfg` shows keys as MISSING, the MFG partition was not flashed correctly or HUK was erased. Re-flash MFG and platform.

## 6. Register in AWS IoT Wireless

If not already registered via the console provisioning flow:

```bash
aws iotwireless create-wireless-device \
  --type Sidewalk \
  --name "evse-monitor-01" \
  --destination-name sidewalk-evse-destination \
  --sidewalk '{"SidewalkManufacturingSn": "<SMSN>"}'
```

Note the returned `Id` (UUID) — this is used by `ota_deploy.py` and `sidewalk_utils.py`.

The device ID is currently hardcoded in `aws/ota_deploy.py`:
```python
DEVICE_ID = "b319d001-6b08-4d88-b4ca-4d2d98a6d43c"
```

For additional devices, update this or use the auto-discovery in `sidewalk_utils.py`.

## 7. Verify End-to-End

1. **Trigger an uplink:**
   ```
   app sid send    # on device serial console
   ```

2. **Check DynamoDB:**
   ```bash
   aws dynamodb query \
     --table-name sidewalk-v1-device_events_v2 \
     --key-condition-expression "device_id = :d" \
     --expression-attribute-values '{":d":{"S":"<device-id>"}}' \
     --scan-index-forward false --limit 1
   ```

3. Verify the item contains `pilot_state`, `pilot_voltage_mv`, `current_ma`, and `thermostat_flags`.

## 8. Capture OTA Baseline

After verifying the device works, capture the current firmware as the OTA baseline for future delta updates:

```bash
python3 aws/ota_deploy.py baseline
```

This reads the device's app partition and uploads it to S3 as the reference for delta OTA.

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `sid status` shows Ready: NO | BLE registration incomplete | Reboot device, wait 60s |
| `sid mfg` shows keys MISSING | MFG partition erased or HUK changed | Re-flash MFG, then platform, then app |
| LoRa: Down after reboot | No Sidewalk gateway in range | Ensure Echo/Ring gateway is powered and nearby |
| No data in DynamoDB | IoT Rule not configured | Check `aws/terraform/` is applied |
| PSA Error -149 in logs | BLE crypto transient error | Normal during BLE reconnects, ignore |
