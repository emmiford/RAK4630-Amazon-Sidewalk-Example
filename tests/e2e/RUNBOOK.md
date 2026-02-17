# E2E Test Runbook

Prerequisites: flashed device, Sidewalk gateway powered, AWS credentials configured.

## 1. Boot & Connect
- [ ] Power device, open serial console (`screen /dev/tty.usbmodem101 115200`)
- [ ] Wait for `Sidewalk READY` log message (< 60s)
- [ ] Run `sid status` — verify "Ready: YES"

## 2. Telemetry Flow (device → cloud)
- [ ] Run `app sid send`
- [ ] Check DynamoDB:
  ```
  aws dynamodb query --table-name sidewalk-v1-device_events_v2 \
    --key-condition-expression "device_id = :d AND #ts > :t" \
    --expression-attribute-names '{"#ts":"timestamp"}' \
    --expression-attribute-values '{":d":{"S":"b319d001-6b08-4d88-b4ca-4d2d98a6d43c"},":t":{"N":"0"}}' \
    --scan-index-forward --limit 1 --no-paginate \
    --query "SortKeyCondition"
  ```
- [ ] Verify latest item has j1772_state, pilot_voltage_mv, current_ma, thermostat fields

## 3. Charge Control (cloud → device)
- [ ] Send pause command:
  ```
  python3 aws/sidewalk_utils.py send 10000000
  ```
- [ ] On device: `app evse status` — verify "Charging allowed: NO"
- [ ] Send allow command:
  ```
  python3 aws/sidewalk_utils.py send 10010000
  ```
- [ ] On device: `app evse status` — verify "Charging allowed: YES"

## 4. OTA Update
- [ ] Run `python3 aws/ota_deploy.py deploy --build --version <N+1>`
- [ ] Monitor: `python3 aws/ota_deploy.py status --watch`
- [ ] Wait for "COMPLETE" status
- [ ] Device reboots — verify new version in `sid status` or boot log

## 5. Sensor Simulation
- [ ] `app evse c` — verify cloud receives J1772 state C within 60s
- [ ] `app evse a` — verify cloud receives J1772 state A within 60s

## 6. Commissioning Self-Test (TASK-048)

### 6a. Boot Self-Test
- [ ] Flash current main (platform + app): `./flash.sh all`
- [ ] Open serial console, observe boot log
- [ ] Confirm self-test runs on power-on — look for `selftest: boot` log lines
- [ ] Verify all checks pass: `adc_pilot OK`, `adc_current OK`, `gpio_heat OK`, `gpio_cool OK`, `charge_en OK`
- [ ] Confirm no error LED pattern (LED2 should not flash)

### 6b. Shell Self-Test
- [ ] Run `sid selftest` via serial console
- [ ] Verify output lists pass/fail for each hardware check
- [ ] All checks should show PASS on a healthy device

### 6c. Fault Flag Uplink
- [ ] Run `app evse c` to simulate State C (charging) — with no actual load attached
- [ ] Wait 10+ seconds for clamp mismatch detection
- [ ] Run `app sid send` to force an uplink
- [ ] Query DynamoDB for latest event:
  ```
  aws dynamodb query --table-name sidewalk-v1-device_events_v2 \
    --key-condition-expression "device_id = :d AND #ts > :t" \
    --expression-attribute-names '{"#ts":"timestamp"}' \
    --expression-attribute-values '{":d":{"S":"b319d001-6b08-4d88-b4ca-4d2d98a6d43c"},":t":{"N":"0"}}' \
    --scan-index-forward --limit 1 --no-paginate
  ```
- [ ] Verify uplink byte 7 has fault bits set (upper nibble)
- [ ] Verify DynamoDB item contains `clamp_mismatch: true`

### 6d. Fault Flag Clearing
- [ ] Run `app evse a` to return to State A (idle, no current)
- [ ] Wait 2 seconds for continuous monitoring to clear the fault
- [ ] Run `app sid send` and query DynamoDB
- [ ] Verify `clamp_mismatch: false` in the new event

## 7. Stale Flash Erase Verification (TASK-022)
- [ ] Flash a large app, then flash a small app using `./flash.sh app`
- [ ] Dump the app partition: `pyocd cmd -t nrf52840 -c "savemem 0x90000 262144 /tmp/partition.bin"`
- [ ] Verify all bytes beyond the small app are 0xFF:
  ```python
  data = open("/tmp/partition.bin", "rb").read()
  # Find end of non-0xFF data
  end = len(data)
  while end > 0 and data[end-1] == 0xFF: end -= 1
  print(f"Non-erased: {end} bytes")
  # Should match small app size, not large app size
  ```
