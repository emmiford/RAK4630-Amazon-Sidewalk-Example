# Product Requirements Document — RAK Sidewalk EVSE Monitor v1.0

**Status**: Retroactive baseline — derived from implemented system as of 2026-02-11
**Hardware**: RAK4631 (nRF52840 + Semtech SX1262 LoRa)
**Connectivity**: Amazon Sidewalk (LoRa 915MHz)
**Cloud**: AWS (Lambda, DynamoDB, S3, IoT Wireless, EventBridge)

---

## 1. Product Overview

### 1.1 Purpose
Monitor residential EV charging equipment (EVSE) and thermostat activity over Amazon Sidewalk's LoRa network. Enable cloud-based demand response by pausing/allowing charging based on time-of-use pricing and grid carbon intensity.

### 1.2 Target Deployment
Single-site residential installation: one RAK4631 device wired to a J1772 EVSE pilot signal, a current clamp on the charging circuit, thermostat call wires, and a relay for charge enable/disable. USB-powered (not battery).

### 1.3 Users
- **Homeowner**: Passive — benefits from demand response without interaction. No user-facing app or dashboard in v1.0.
- **Operator/Developer**: Active — uses shell commands (USB serial) for diagnostics, OTA deploy CLI for firmware updates, AWS console/CLI for cloud monitoring.

---

## 2. Device Requirements

### 2.1 J1772 Pilot State Monitoring

| Requirement | Status |
|-------------|--------|
| Read J1772 pilot voltage via ADC (AIN0, 12-bit, 0-3.6V range) | IMPLEMENTED |
| Classify into states A through F using voltage thresholds | IMPLEMENTED |
| State A: >2600 mV (not connected) | IMPLEMENTED |
| State B: 1850-2600 mV (connected, not ready) | IMPLEMENTED |
| State C: 1100-1850 mV (charging) | IMPLEMENTED |
| State D: 350-1100 mV (charging with ventilation required) | IMPLEMENTED |
| State E: <350 mV (error — short circuit) | IMPLEMENTED |
| State F: no pilot signal (EVSE error) | IMPLEMENTED |
| Poll interval: 500ms | IMPLEMENTED |
| Transmit on state change only (not every poll) | IMPLEMENTED |
| Simulation mode for testing (states A-D, 10s duration) | IMPLEMENTED |

**Notes**: Voltage thresholds are hardcoded and have not been calibrated against physical J1772 hardware. See Oliver REC-002 for recommended validation.

### 2.2 Current Clamp Monitoring

| Requirement | Status |
|-------------|--------|
| Read current clamp voltage via ADC (AIN1, 12-bit) | IMPLEMENTED |
| Linear scaling: 0-3.3V = 0-30A (0-30,000 mA) | IMPLEMENTED |
| Transmit on change detection | IMPLEMENTED |

**Notes**: Calibration assumes perfect linear clamp response. No field calibration procedure exists.

### 2.3 Thermostat Input Monitoring

| Requirement | Status |
|-------------|--------|
| Read heat call signal (GPIO P0.04, active high, pull-down) | IMPLEMENTED |
| Read cool call signal (GPIO P0.05, active high, pull-down) | IMPLEMENTED |
| Pack as 2-bit flag field in uplink payload | IMPLEMENTED |
| Transmit on change detection | IMPLEMENTED |

### 2.4 Charge Enable/Disable Output

| Requirement | Status |
|-------------|--------|
| GPIO output for charge relay (P0.06, active high) | IMPLEMENTED |
| Allow/pause via cloud downlink command (0x10) | IMPLEMENTED |
| Allow/pause via shell command (`app evse allow/pause`) | IMPLEMENTED |
| Auto-resume timer (configurable duration in minutes) | IMPLEMENTED |
| Default state on boot: charging allowed | IMPLEMENTED |

### 2.5 LED Indicators

| Requirement | Status |
|-------------|--------|
| LED control via app_leds module | IMPLEMENTED |
| Status indication (Sidewalk connected, error, etc.) | IMPLEMENTED |

### 2.6 Shell Diagnostics

| Requirement | Status |
|-------------|--------|
| `sid status` — Sidewalk connection state, link type, init status | IMPLEMENTED |
| `sid mfg` — MFG store version and device ID | IMPLEMENTED |
| `sid ota status` — OTA state machine phase | IMPLEMENTED |
| `app evse status` — J1772 state, voltage, current, charge control | IMPLEMENTED |
| `app evse a/b/c/d` — Simulate J1772 states | IMPLEMENTED |
| `app evse allow/pause` — Manual charge control | IMPLEMENTED |
| `app hvac status` — Thermostat heat/cool flags | IMPLEMENTED |
| `app sid send` — Manual uplink trigger | IMPLEMENTED |
| USB CDC ACM serial console (115200 baud) | IMPLEMENTED |

---

## 3. Connectivity Requirements

### 3.1 Sidewalk LoRa Link

| Requirement | Status |
|-------------|--------|
| Amazon Sidewalk LoRa 915MHz via SX1262 | IMPLEMENTED |
| BLE registration on first boot | IMPLEMENTED |
| LoRa data link on subsequent boots | IMPLEMENTED |
| Link mask switching (BLE/LoRa/auto) via shell | IMPLEMENTED |
| TCXO patch for RAK4631 radio stability | IMPLEMENTED |
| MFG key health check at boot (detect missing credentials) | IMPLEMENTED |

### 3.2 Uplink (Device → Cloud)

| Requirement | Status |
|-------------|--------|
| Raw 8-byte payload: `[0xE5, ver, J1772, volt_lo, volt_hi, curr_lo, curr_hi, thermo]` | IMPLEMENTED |
| Transmit on sensor state change | IMPLEMENTED |
| 60-second heartbeat (transmit even if no change) | IMPLEMENTED |
| 100ms minimum TX rate limiter | IMPLEMENTED |
| Fits within 19-byte Sidewalk LoRa MTU | IMPLEMENTED |

### 3.3 Downlink (Cloud → Device)

| Requirement | Status |
|-------------|--------|
| Charge control command: `[0x10, allowed, 0x00, 0x00]` | IMPLEMENTED |
| OTA commands: `[0x20, sub_cmd, ...]` | IMPLEMENTED |
| Unknown command logging (graceful reject) | IMPLEMENTED |

---

## 4. Cloud Requirements

### 4.1 Payload Decode

| Requirement | Status |
|-------------|--------|
| Lambda decodes raw 8-byte EVSE payload | IMPLEMENTED |
| Lambda decodes legacy sid_demo format (backward compat) | IMPLEMENTED |
| Decoded events stored in DynamoDB (device_id + timestamp) | IMPLEMENTED |
| TTL expiration on DynamoDB records | IMPLEMENTED |
| OTA ACK detection triggers OTA sender Lambda | IMPLEMENTED |

### 4.2 OTA Firmware Update Pipeline

| Requirement | Status |
|-------------|--------|
| S3 upload triggers OTA sender Lambda | IMPLEMENTED |
| Chunk delivery: 15-byte data + 4-byte header = 19-byte MTU | IMPLEMENTED |
| Delta mode: compare against S3 baseline, send only changed chunks | IMPLEMENTED |
| Full mode: send all chunks (fallback) | IMPLEMENTED |
| Session state tracked in DynamoDB (sentinel key timestamp=-1) | IMPLEMENTED |
| EventBridge retry timer (1-minute interval) | IMPLEMENTED |
| Stale session detection (30s threshold) | IMPLEMENTED |
| Max 5 retries before abort | IMPLEMENTED |
| CloudWatch alarms for Lambda errors and missing invocations | IMPLEMENTED |
| Deploy CLI: `ota_deploy.py baseline/deploy/preview/status/abort` | IMPLEMENTED |

### 4.3 Device-Side OTA

| Requirement | Status |
|-------------|--------|
| OTA state machine: IDLE → RECEIVING → VALIDATING → APPLYING → reboot | IMPLEMENTED |
| CRC32 validation on complete image | IMPLEMENTED |
| Flash staging area at 0xD0000 (148KB) | IMPLEMENTED |
| Delta mode: bitfield tracking of received chunks | IMPLEMENTED |
| Delta mode: merged CRC validation (staging + primary) | IMPLEMENTED |
| Deferred apply (15s delay for COMPLETE uplink to transmit) | IMPLEMENTED |
| Recovery metadata at 0xCFF00 (survives power loss during apply) | IMPLEMENTED |
| Boot recovery: resume interrupted flash copy | IMPLEMENTED |
| Duplicate/stale START rejection (CRC match = already applied) | IMPLEMENTED |
| Pre-apply hook: stop app callbacks before flash copy | IMPLEMENTED |

### 4.4 Demand Response Charge Scheduling

| Requirement | Status |
|-------------|--------|
| EventBridge-triggered Lambda (configurable rate) | IMPLEMENTED |
| Xcel Colorado TOU peak detection (weekdays 5-9 PM MT) | IMPLEMENTED |
| WattTime MOER grid signal for PSCO region | IMPLEMENTED |
| Decision: pause if TOU peak OR MOER > threshold | IMPLEMENTED |
| State deduplication: skip downlink if command unchanged | IMPLEMENTED |
| DynamoDB state tracking (sentinel key timestamp=0) | IMPLEMENTED |
| Audit log: every command sent logged with real timestamp | IMPLEMENTED |
| Send charge control downlink via IoT Wireless | IMPLEMENTED |
| MOER threshold configurable (env var, default 70%) | IMPLEMENTED |

**Notes**: The 70% MOER threshold is unvalidated against real PSCO data. See Oliver REC-004 / TASK-012.

### 4.5 Infrastructure as Code

| Requirement | Status |
|-------------|--------|
| All AWS resources defined in Terraform | IMPLEMENTED |
| IAM roles with least-privilege policies | IMPLEMENTED |
| CloudWatch log groups with 14-day retention | IMPLEMENTED |
| S3 bucket for firmware binaries | IMPLEMENTED |
| DynamoDB with PAY_PER_REQUEST billing | IMPLEMENTED |

---

## 5. Operational Requirements

### 5.1 Firmware Update

| Requirement | Status |
|-------------|--------|
| App-only OTA over Sidewalk LoRa (no physical access needed) | IMPLEMENTED |
| Platform update requires physical programmer (pyOCD) | IMPLEMENTED |
| Delta OTA for fast incremental updates (~seconds) | IMPLEMENTED |
| Full OTA fallback (~69 minutes) | IMPLEMENTED |
| Recovery from power loss during apply | IMPLEMENTED |
| OTA recovery runbook / operator documentation | NOT STARTED |

### 5.2 Device Provisioning

| Requirement | Status |
|-------------|--------|
| Sidewalk credentials in MFG partition (0xFF000) | IMPLEMENTED |
| `credentials.example/` template directory | IMPLEMENTED |
| Flash script for all partitions (`flash.sh all`) | IMPLEMENTED |
| Provisioning documentation (step-by-step workflow) | NOT STARTED |

### 5.3 Observability

| Requirement | Status |
|-------------|--------|
| Shell diagnostics over USB serial | IMPLEMENTED |
| Sidewalk init status tracking (7 states, shell-queryable) | IMPLEMENTED |
| MFG key health check at boot | IMPLEMENTED |
| CloudWatch Lambda logs (14-day retention) | IMPLEMENTED |
| CloudWatch alarms for OTA sender errors and stalls | IMPLEMENTED |
| Cloud-side OTA status monitoring (`ota_deploy.py status`) | IMPLEMENTED |
| DynamoDB event history (queryable per device) | IMPLEMENTED |
| Dashboard or alerting for device offline detection | NOT STARTED |

### 5.4 Testing

| Requirement | Status |
|-------------|--------|
| Host-side C unit tests (Grenning dual-target) | IMPLEMENTED (32 tests) |
| OTA sender Lambda tests | IMPLEMENTED |
| Charge scheduler Lambda tests | NOT STARTED |
| Decode Lambda tests | NOT STARTED |
| OTA recovery path tests | NOT STARTED |
| CI/CD pipeline (automated test on push/PR) | NOT STARTED |
| E2E test plan (device → cloud → device) | NOT STARTED |
| Field reliability testing across RF conditions | NOT STARTED |

---

## 6. Non-Functional Requirements

### 6.1 Power

| Requirement | Status |
|-------------|--------|
| USB-powered (not battery) | IMPLEMENTED |
| No specific power budget target | N/A |

**Notes**: Device is USB-powered via the RAK4631 USB-C port. Battery optimization is not a v1.0 concern but becomes relevant if future deployment uses battery power.

### 6.2 Reliability

| Requirement | Status |
|-------------|--------|
| OTA recovery from power loss during apply | IMPLEMENTED |
| Lost ACK recovery via cloud retry timer | IMPLEMENTED |
| Stale session detection and abort | IMPLEMENTED |
| Heartbeat for liveness detection (60s) | IMPLEMENTED |
| Quantified reliability targets (uptime %, delivery rate) | NOT STARTED |
| Field-tested under real LoRa conditions | NOT STARTED |

### 6.3 Security

| Requirement | Status |
|-------------|--------|
| Sidewalk protocol encryption (built-in) | IMPLEMENTED |
| MFG credentials in dedicated flash partition | IMPLEMENTED |
| HUK for PSA crypto key derivation | IMPLEMENTED |
| MFG key health check (detect empty/missing keys) | IMPLEMENTED |
| OTA image CRC32 validation | IMPLEMENTED |
| OTA image cryptographic signing | NOT STARTED |
| Credential rotation procedure | NOT STARTED |

**Notes**: OTA images are validated by CRC32 only — no cryptographic signature. A compromised S3 bucket or Lambda could push malicious firmware. This is acceptable for single-site development but would need signing for production deployment.

---

## 7. Scope Boundaries

### 7.1 In Scope for v1.0
- Single-device, single-site deployment
- J1772 monitoring, current sensing, thermostat inputs
- Charge control via Sidewalk downlink
- Demand response (TOU + WattTime MOER)
- App-only OTA over LoRa with delta mode
- Shell diagnostics over USB
- AWS cloud infrastructure (Terraform-managed)
- Host-side unit tests

### 7.2 Out of Scope for v1.0
- Multi-device fleet management
- User-facing mobile app or web dashboard
- Battery-powered operation
- BLE-only mode (LoRa is primary)
- OTA image cryptographic signing
- On-device data logging (device is stateless, cloud stores everything)
- OCPP (Open Charge Point Protocol) integration
- Solar/battery storage integration
- Multi-tariff or multi-utility TOU schedules (Xcel Colorado only)

### 7.3 Future Considerations
- Fleet provisioning and management tooling
- Dashboard for real-time monitoring (DynamoDB → API Gateway → frontend)
- OTA image signing with ED25519 (keys already in MFG store)
- Battery-powered variant with sleep modes and reduced heartbeat
- Additional utility TOU schedules beyond Xcel Colorado
- OCPP gateway for commercial EVSE integration

---

## 8. Known Gaps

| Gap | Impact | Backlog Task |
|-----|--------|-------------|
| No PRD existed until this document | Scope ambiguity | TASK-014 (this document) |
| Charge scheduler Lambda has no tests | Risk of silent breakage | TASK-004 |
| Decode Lambda has no tests | Risk of silent breakage | TASK-006 |
| OTA recovery path has no tests | Safety-critical code untested | TASK-005 |
| MOER threshold (70%) unvalidated | May over/under-curtail charging | TASK-012 |
| OTA not field-tested under real RF | Risk of bricking at range | TASK-013 |
| No CI/CD pipeline | Manual test discipline required | TASK-009, TASK-010 |
| No provisioning documentation | Bus factor risk | TASK-011 |
| No OTA recovery runbook | Operator cannot diagnose failures | TASK-008 |
| No architecture documentation | SDK divergence knowledge is tribal | TASK-016 |
| Dead sid_demo_parser code (~1,600 lines) | Confusion, binary bloat | TASK-015 |
| Legacy demo app in tree | Confusion | TASK-017 |
| No OTA image signing | Compromised S3 could push bad firmware | Future (out of v1.0 scope) |
| No device offline alerting | Silent failures undetected | Future |
| J1772 thresholds not hardware-calibrated | Possible misclassification | Oliver REC-002 |

---

## 9. Requirement Traceability

Every backlog task maps to a gap in this PRD:

| Task | PRD Section | Requirement |
|------|-------------|-------------|
| TASK-003 | 1.0 (Overview) | Architecture documentation for new devs |
| TASK-004 | 5.4 (Testing) | Charge scheduler Lambda tests |
| TASK-005 | 5.4 (Testing) | OTA recovery path tests |
| TASK-006 | 5.4 (Testing) | Decode Lambda tests |
| TASK-007 | 5.4 (Testing) | E2E test plan |
| TASK-008 | 5.1 (Firmware Update) | OTA recovery runbook |
| TASK-009 | 5.4 (Testing) | CI/CD for C unit tests |
| TASK-010 | 5.4 (Testing) | CI/CD for Lambda tests |
| TASK-011 | 5.2 (Provisioning) | Provisioning documentation |
| TASK-012 | 4.4 (Demand Response) | MOER threshold validation |
| TASK-013 | 6.2 (Reliability) | OTA field reliability testing |
| TASK-015 | — (Cleanup) | Dead code removal |
| TASK-016 | 3.1 / 5.3 (SDK/Observability) | Architecture decisions documentation |
| TASK-017 | — (Cleanup) | Legacy app removal |
