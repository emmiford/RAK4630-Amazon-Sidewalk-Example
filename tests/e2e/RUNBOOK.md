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
