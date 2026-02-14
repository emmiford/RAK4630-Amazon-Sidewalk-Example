# SideCharge Project Plan

**Author**: Malcolm (Senior Project Manager, Eta Works)
**Created**: 2026-02-12
**Source**: PRD v1.4, task list (2026-02-11), known issues, architecture doc, ADR-001
**Purpose**: Master project plan from current state to production deployment

---

## How to Read This Plan

- **[x]** = Task is IMPLEMENTED and verified
- **[ ]** = Task is NOT YET DONE (designed, not started, or TBD)
- **Status tags**: `IMPLEMENTED`, `DESIGNED`, `NOT STARTED`, `TBD`, `PARTIAL`
- **Type tags**: `HW` = hardware, `SW` = firmware/software, `CLOUD` = AWS/Lambda, `DOC` = documentation
- **Complexity**: S (1-2 days), M (3-5 days), L (1-2 weeks), XL (2-4 weeks)
- **Dependencies** are noted inline as `Depends: [task reference]`

---

## Epic 1: Circuit Interlock Core

The core product function. The interlock guarantees AC and EV charger never draw simultaneously, enabling a Level 2 charger on a shared circuit without a panel upgrade. This covers PRD sections 2.0.x.

### Milestone 1.1: Hardware Interlock (Mutual Exclusion)

#### Feature 1.1.1: Hardware-Enforced Mutual Exclusion
- **Story**: As a homeowner, I want the interlock to prevent AC and EV charger from running at the same time even if the microcontroller crashes, so that the circuit breaker never trips under normal operation. **(L)**
  - [x] Task: Implement hardware circuit that physically blocks simultaneous operation independent of MCU — `HW` — IMPLEMENTED (SW+HW)
  - [x] Task: Implement AC priority logic — thermostat call pauses EV charging — `HW+SW` — IMPLEMENTED (SW+HW)
  - [x] Task: Implement EV lockout — current flowing to EV blocks thermostat signal to compressor — `HW+SW` — IMPLEMENTED (SW+HW)
  - [x] Task: Verify fail-safe behavior when MCU loses power (HW interlock still active) — `HW` — IMPLEMENTED (HW)

#### Feature 1.1.2: Asymmetric Interlock Mechanism
- **Story**: As a system, I want to use protocol-aware methods to pause each load (J1772 spoofing for EV, signal blocking for AC), so that pausing is graceful and charger-safe. **(M)**
  - [x] Task: Implement local interlock EV pause — present ~900 ohm resistance for J1772 State B — `HW+SW` — IMPLEMENTED (SW+HW)
  - [ ] Task: Implement cloud/utility override EV pause — set J1772 PWM duty cycle to 0% — `SW` — NOT STARTED — Depends: EXP-001 (car make testing). PRD 2.0.1 notes this is not finalized.
  - [x] Task: Implement AC pause — block 24VAC thermostat call signal to compressor contactor — `HW` — IMPLEMENTED (HW)

#### Feature 1.1.3: Isolation and Safety
- **Story**: As an installer, I want the device to be galvanically isolated from all high-voltage circuits, so that a fault on one circuit cannot propagate to others. **(M)**
  - [x] Task: Isolate MCU from 24VAC thermostat circuit (optocouplers) — `HW` — IMPLEMENTED (HW)
  - [x] Task: Isolate MCU from J1772 pilot circuit (+/-12V) — `HW` — IMPLEMENTED (HW)
  - [x] Task: Establish earth ground reference from EV charger enclosure — `HW` — IMPLEMENTED (HW)
  - [x] Task: Galvanic isolation between 24VAC AC power and 240VAC charging circuit — `HW` — IMPLEMENTED (HW)

### Milestone 1.2: Software Interlock Layer

#### Feature 1.2.1: Boot and Power Recovery Behavior
- **Story**: As a system, I want to read the thermostat state before enabling EV charging on every boot, so that the software never contradicts the hardware interlock. **(M)**
  - [ ] Task: Read cool_call GPIO before setting charge enable in `charge_control_init()` — `SW` — NOT STARTED — File: `charge_control.c`
  - [ ] Task: Change `platform_gpio_init()` from `GPIO_OUTPUT_ACTIVE` to `GPIO_OUTPUT_INACTIVE` — `SW` — NOT STARTED — File: `platform_api_impl.c`
  - [ ] Task: Add charge_control state to uplink payload — `SW` — NOT STARTED — File: `app_tx.c`
  - [ ] Task: Log boot decision at INF level — `SW` — NOT STARTED — File: `charge_control.c`
  - [ ] Task: Verify all edge cases: unconnected GPIOs, power cycle during AC call, power cycle during EV charging, power cycle during Charge Now override, power cycle during cloud override, watchdog reset — `SW` — NOT STARTED

#### Feature 1.2.2: "Charge Now" Override Button
- **Story**: As a homeowner, I want to press a physical button to temporarily override AC priority and force-charge my EV, so that I can top up for an urgent trip. **(M)**
  - [ ] Task: Implement momentary push button handler — single press = activate 30-min override — `SW+HW` — DESIGNED — Duration: 30 min (current recommendation, not finalized per PRD 2.0.1.1)
  - [ ] Task: Implement long press (3 seconds) = cancel override early — `SW` — DESIGNED
  - [ ] Task: Implement thermal comfort safeguard — auto-cancel if AC demands continuously for 30 min — `SW` — DESIGNED
  - [ ] Task: Override state is RAM-only — power cycle clears it — `SW` — DESIGNED
  - [ ] Task: Report override activation/cancellation in next uplink — `SW` — NOT STARTED
  - [ ] Task: Long press (10 seconds) = activate BLE diagnostics beacon — `SW` — DESIGNED — Depends: BLE diagnostics (Epic 9)

#### Feature 1.2.3: Cloud Override (EV Charger)
- **Story**: As a cloud operator, I want to remotely pause or allow EV charging with a configurable timeout, so that the cloud can implement demand response without permanently blocking charging. **(S)**
  - [x] Task: Implement force-block EV charger via cloud downlink command 0x10 — `SW` — IMPLEMENTED (SW)
  - [x] Task: Implement auto-resume timer (configurable duration in minutes) — `SW` — IMPLEMENTED (SW)
  - [ ] Task: Implement force-block AC via cloud downlink — `SW` — NOT STARTED — PRD 2.0.2: requires careful safety review before implementation

#### Feature 1.2.4: Interlock Logging and Reporting
- **Story**: As an operator, I want interlock state transitions logged to the cloud with timestamps and reasons, so that I can demonstrate code compliance and debug issues. **(M)**
  - [x] Task: Include interlock state in uplink payload — `SW` — IMPLEMENTED (SW)
  - [x] Task: Include cloud override status in uplink payload — `SW` — IMPLEMENTED (SW, EV only)
  - [ ] Task: Log interlock transition events to cloud (AC->EV, EV->AC, override) with timestamps — `SW+CLOUD` — NOT STARTED
  - [ ] Task: Log "Charge Now" button press events — `SW` — TBD

---

## Epic 2: Sensor & Monitoring

All analog and digital sensor inputs: J1772 pilot monitoring (2.1), current clamp (2.2), thermostat inputs (2.3), and charge enable/disable output (2.4). Covers calibration, scaling, and field validation.

### Milestone 2.1: J1772 Pilot State Monitoring

#### Feature 2.1.1: Pilot Voltage Classification
- **Story**: As a system, I want to read the J1772 pilot voltage and classify it into states A-F, so that the interlock knows whether a car is connected and charging. **(S)**
  - [x] Task: Read J1772 pilot voltage via ADC (AIN0, 12-bit, 0-3.6V range) — `SW+HW` — IMPLEMENTED
  - [x] Task: Classify into states A-F using voltage thresholds — `SW` — IMPLEMENTED
  - [x] Task: Implement all state ranges: A (>2600mV), B (1850-2600), C (1100-1850), D (350-1100), E (<350), F (no pilot) — `SW` — IMPLEMENTED
  - [x] Task: Poll interval 500ms — `SW` — IMPLEMENTED
  - [x] Task: Transmit on state change only — `SW` — IMPLEMENTED
  - [x] Task: Simulation mode for testing (states A-D, 10s duration) — `SW` — IMPLEMENTED

#### Feature 2.1.2: J1772 PWM Duty Cycle Decoding
- **Story**: As a system, I want to decode the J1772 PWM duty cycle, so that I know the maximum current the charger is offering and can report it to the cloud. **(M)**
  - [ ] Task: Implement Cp PWM duty cycle measurement on AIN0 (same pin as voltage, separate measurement) — `SW` — NOT STARTED — PRD ref: TASK-022 in known gaps
  - [ ] Task: Map duty cycle to max allowed current per J1772 spec — `SW` — NOT STARTED
  - [ ] Task: Include PWM duty cycle / max current in uplink payload (if payload space permits) — `SW` — NOT STARTED

#### Feature 2.1.3: Field Calibration of J1772 Thresholds
- **Story**: As an operator, I want voltage thresholds validated against real J1772 hardware (not just simulated signals), so that state classification is reliable in production. **(M)**
  - [ ] Task: Test voltage thresholds with at least 3 different physical J1772 chargers — `HW` — NOT STARTED — PRD ref: Oliver REC-002
  - [ ] Task: Test car-side interlock mechanism (PWM 0% approach) with multiple car makes — `HW` — NOT STARTED — PRD ref: Oliver EXP-001
  - [ ] Task: Adjust thresholds if field data diverges from datasheet values — `SW` — NOT STARTED

### Milestone 2.2: Current Clamp Monitoring

#### Feature 2.2.1: Current Measurement and Scaling
- **Story**: As a system, I want to measure real-time EV charging current via the current clamp, so that I can report charging power and detect anomalies. **(M)**
  - [x] Task: Read current clamp voltage via ADC (AIN1, 12-bit) — `SW+HW` — IMPLEMENTED
  - [x] Task: Linear scaling 0-3.3V = 0-30A (0-30,000 mA) — `SW` — IMPLEMENTED (needs rescaling)
  - [x] Task: Transmit on change detection — `SW` — IMPLEMENTED
  - [ ] Task: Rescale to minimum 48A range (60A circuit at 80% continuous) — `SW+HW` — NOT STARTED — PRD ref: TASK-026 in known gaps. Requires clamp selection + resistor divider change.
  - [ ] Task: Evaluate 80A range for Ford Charge Station Pro support — `HW` — TBD
  - [ ] Task: Develop field calibration procedure (real clamp nonlinearities at extremes) — `DOC+SW` — NOT STARTED

### Milestone 2.3: Thermostat Input Monitoring

#### Feature 2.3.1: Thermostat Signal Reading
- **Story**: As a system, I want to read both cool and heat call signals from the thermostat, so that the interlock can detect when the AC compressor needs to run. **(S)**
  - [x] Task: Read cool call signal (GPIO P0.05, active high, pull-down) — `SW+HW` — IMPLEMENTED
  - [x] Task: Read heat call signal (GPIO P0.04, active high, pull-down) — `SW+HW` — IMPLEMENTED (monitored, not used for interlock in v1.0)
  - [x] Task: Pack as 2-bit flag field in uplink payload — `SW` — IMPLEMENTED
  - [x] Task: Transmit on change detection — `SW` — IMPLEMENTED

### Milestone 2.4: Charge Enable/Disable Output

#### Feature 2.4.1: Charge Relay Control
- **Story**: As a system, I want to control the charge enable GPIO to allow or pause EV charging, so that interlock decisions are enforced electrically. **(S)**
  - [x] Task: GPIO output for charge relay (P0.06, active high) — `SW+HW` — IMPLEMENTED
  - [x] Task: Allow/pause via cloud downlink command 0x10 — `SW` — IMPLEMENTED
  - [x] Task: Auto-resume timer (configurable duration in minutes) — `SW` — IMPLEMENTED

---

## Epic 3: Connectivity & Cloud

Amazon Sidewalk integration, uplink/downlink messaging, payload encoding, AWS backend (Lambda, DynamoDB, S3), and error handling. Covers PRD sections 3.x and 4.x.

### Milestone 3.1: Sidewalk LoRa Link

#### Feature 3.1.1: Sidewalk Connectivity
- **Story**: As a device, I want to connect to Amazon Sidewalk via LoRa 915MHz, so that I can send telemetry and receive commands without WiFi, cellular, or monthly fees. **(S)**
  - [x] Task: Amazon Sidewalk LoRa 915MHz via SX1262 — `SW+HW` — IMPLEMENTED
  - [x] Task: BLE registration (first boot only) — `SW` — IMPLEMENTED
  - [x] Task: LoRa data link on subsequent boots — `SW` — IMPLEMENTED
  - [x] Task: TCXO patch for RAK4631 radio stability — `SW` — IMPLEMENTED
  - [x] Task: MFG key health check at boot (detect missing/empty credentials) — `SW` — IMPLEMENTED
  - [x] Task: Sidewalk session key presence check (trigger BLE registration if needed) — `SW` — IMPLEMENTED
  - [x] Task: Unknown command type logging (graceful reject) — `SW` — IMPLEMENTED

#### Feature 3.1.2: Reconnection and Error Handling
- **Story**: As a device, I want to handle connectivity loss gracefully and continue local interlock operation, so that a network outage never disables the safety function. **(M)**
  - [x] Task: Sidewalk init status tracking (7 states, shell-queryable) — `SW` — IMPLEMENTED
  - [ ] Task: Implement reconnection strategy after extended Sidewalk disconnect — `SW` — NOT STARTED
  - [ ] Task: Define behavior when Sidewalk not ready 10+ minutes after boot (error state for LED) — `SW` — DESIGNED (LED matrix section 2.5.1)

### Milestone 3.2: Uplink (Device to Cloud)

#### Feature 3.2.1: Telemetry Payload
- **Story**: As a cloud system, I want to receive structured 12-byte payloads with timestamps from the device, so that I can reconstruct a timeline of interlock events. **(M)**
  - [x] Task: Implement 8-byte EVSE payload (magic, version, J1772 state, voltage, current, thermostat flags) — `SW` — IMPLEMENTED
  - [ ] Task: Expand payload to 12 bytes — add 4-byte SideCharge epoch timestamp — `SW` — NOT STARTED — PRD ref: TASK-035
  - [ ] Task: Add charge_control state (bit 2) and Charge Now flag (bit 3) to thermostat flags byte — `SW` — NOT STARTED — PRD ref: TASK-035
  - [ ] Task: Bump payload version to 0x07 — `SW` — NOT STARTED
  - [x] Task: Transmit on sensor state change — `SW` — IMPLEMENTED
  - [ ] Task: Implement 15-minute heartbeat (currently 60s testing interval) — `SW` — NOT STARTED
  - [x] Task: 100ms minimum TX rate limiter — `SW` — IMPLEMENTED
  - [x] Task: Verify fits within 19-byte Sidewalk LoRa MTU (12 bytes = 7 spare) — `SW` — IMPLEMENTED

#### Feature 3.2.2: Device-Side Event Buffer
- **Story**: As a device, I want to buffer timestamped state snapshots in a ring buffer, so that transient state changes are not lost when LoRa drops a message. **(M)**
  - [ ] Task: Implement ring buffer of 12-byte timestamped state snapshots in RAM (50 entries = 600 bytes from 8KB budget) — `SW` — NOT STARTED — PRD ref: TASK-034
  - [ ] Task: On each uplink, send most recent state snapshot — `SW` — NOT STARTED
  - [ ] Task: Trim buffer entries at or before cloud ACK watermark timestamp — `SW` — NOT STARTED — Depends: TIME_SYNC downlink (Feature 3.3.1)
  - [ ] Task: Ring buffer wraps and overwrites oldest entries when full — `SW` — NOT STARTED

### Milestone 3.3: Downlink (Cloud to Device)

#### Feature 3.3.1: TIME_SYNC Command (0x30)
- **Story**: As a device, I want to receive wall-clock time from the cloud, so that I can timestamp my uplink payloads with real time. **(M)**
  - [ ] Task: Implement TIME_SYNC downlink command (0x30) parsing — `SW` — NOT STARTED — PRD ref: TASK-033
  - [ ] Task: Implement device-side time tracking (sync_time + uptime offset) — `SW` — NOT STARTED
  - [ ] Task: Implement cloud-side auto-sync on first uplink after boot (decode Lambda detects timestamp=0) — `CLOUD` — NOT STARTED
  - [ ] Task: Implement periodic drift correction (daily TIME_SYNC) — `CLOUD` — NOT STARTED
  - [ ] Task: Include ACK watermark in TIME_SYNC for event buffer trimming — `CLOUD` — NOT STARTED

#### Feature 3.3.2: Charge Control Downlink (0x10)
- **Story**: As a cloud system, I want to send charge control commands to the device, so that the demand response scheduler can pause or allow charging remotely. **(S)**
  - [x] Task: Implement 0x10 charge control downlink parsing (allow/pause) — `SW` — IMPLEMENTED
  - [x] Task: Implement auto-resume timer as safety net — `SW` — IMPLEMENTED

### Milestone 3.4: Cloud Payload Processing

#### Feature 3.4.1: Decode Lambda
- **Story**: As a cloud system, I want to decode raw device payloads into structured events in DynamoDB, so that all telemetry is queryable and actionable. **(S)**
  - [x] Task: Lambda decodes raw 8-byte EVSE payload — `CLOUD` — IMPLEMENTED
  - [x] Task: Decoded events stored in DynamoDB (device_id + timestamp) — `CLOUD` — IMPLEMENTED
  - [x] Task: TTL expiration on DynamoDB records — `CLOUD` — IMPLEMENTED
  - [x] Task: Backward compatibility via payload version field (byte 1) — `CLOUD` — DESIGNED
  - [ ] Task: Update decode Lambda to handle v0x07 12-byte payload with timestamp — `CLOUD` — NOT STARTED — Depends: Feature 3.2.1 (payload expansion)
  - [ ] Task: Update decode Lambda to update device registry `last_seen` and `app_version` — `CLOUD` — NOT STARTED — Depends: Epic 9, Feature 9.2.1

#### Feature 3.4.2: Demand Response and Charge Scheduling
- **Story**: As a cloud system, I want to automatically pause EV charging during peak electricity hours and high grid carbon periods, so that charging shifts to off-peak, low-carbon windows. **(S)**
  - [x] Task: EventBridge-triggered Lambda (configurable rate) — `CLOUD` — IMPLEMENTED
  - [x] Task: Xcel Colorado TOU peak detection (weekdays 5-9 PM MT) — `CLOUD` — IMPLEMENTED
  - [x] Task: WattTime MOER grid signal for PSCO region — `CLOUD` — IMPLEMENTED
  - [x] Task: Decision logic: pause if TOU peak OR MOER > threshold — `CLOUD` — IMPLEMENTED
  - [x] Task: State deduplication: skip downlink if command unchanged — `CLOUD` — IMPLEMENTED
  - [x] Task: DynamoDB state tracking (sentinel key) — `CLOUD` — IMPLEMENTED
  - [x] Task: Audit log: every command logged with real timestamp — `CLOUD` — IMPLEMENTED
  - [x] Task: Send charge control downlink via IoT Wireless — `CLOUD` — IMPLEMENTED
  - [x] Task: MOER threshold configurable (env var, default 70%) — `CLOUD` — IMPLEMENTED

#### Feature 3.4.3: Infrastructure as Code
- **Story**: As a developer, I want all AWS resources defined in Terraform, so that the infrastructure is reproducible and auditable. **(S)**
  - [x] Task: All AWS resources defined in Terraform — `CLOUD` — IMPLEMENTED
  - [x] Task: IAM roles with least-privilege policies — `CLOUD` — IMPLEMENTED
  - [x] Task: CloudWatch log groups with 14-day retention — `CLOUD` — IMPLEMENTED
  - [x] Task: S3 bucket for firmware binaries — `CLOUD` — IMPLEMENTED
  - [x] Task: DynamoDB with PAY_PER_REQUEST billing — `CLOUD` — IMPLEMENTED

---

## Epic 4: OTA & Firmware

Split-image architecture, delta OTA, recovery, staging partition, deploy tooling, and the stale flash fix. Covers PRD sections 4.2, 4.3, 5.1, and known issue KI-003.

### Milestone 4.1: Split-Image Architecture

#### Feature 4.1.1: Platform/App Separation
- **Story**: As a developer, I want the platform and app to be independently compiled and linked, so that the app can be OTA-updated without reflashing the entire firmware. **(S)**
  - [x] Task: Platform at 0x00000 (576KB) — Zephyr RTOS + Sidewalk + OTA engine — `SW` — IMPLEMENTED
  - [x] Task: App at 0x90000 (256KB) — EVSE domain logic (~4KB) — `SW` — IMPLEMENTED
  - [x] Task: Versioned function pointer tables at fixed addresses (platform_api at 0x8FF00, app_callbacks at 0x90000) — `SW` — IMPLEMENTED
  - [x] Task: Magic word and version validation at boot — `SW` — IMPLEMENTED
  - [x] Task: API version mismatch is a hard stop (ADR-001) — `SW` — IMPLEMENTED — `docs/adr/001-version-mismatch-hard-stop.md`

#### Feature 4.1.2: Platform API Contract
- **Story**: As an app developer, I want a stable, well-documented API contract with the platform, so that I can develop and test the app without the platform. **(S)**
  - [x] Task: 22 platform function pointers (Sidewalk, Hardware, System, Timer, Logging, Shell, Memory, MFG) — `SW` — IMPLEMENTED
  - [x] Task: 7 app callback pointers (init, on_ready, on_msg_received, on_msg_sent, on_send_error, on_timer, on_shell_cmd) — `SW` — IMPLEMENTED
  - [x] Task: Mock platform API for host-side testing (mock_platform_api.c) — `SW` — IMPLEMENTED

### Milestone 4.2: OTA Pipeline (Cloud Side)

#### Feature 4.2.1: OTA Sender Lambda
- **Story**: As an operator, I want to push firmware updates to deployed devices over LoRa, so that I can patch bugs without physical access. **(S)**
  - [x] Task: S3 upload triggers OTA sender Lambda — `CLOUD` — IMPLEMENTED
  - [x] Task: OTA ACK detection in decode Lambda triggers OTA sender — `CLOUD` — IMPLEMENTED
  - [x] Task: Chunk delivery: 15-byte data + 4-byte header = 19-byte MTU — `CLOUD` — IMPLEMENTED
  - [x] Task: Delta mode: compare against S3 baseline, send only changed chunks — `CLOUD` — IMPLEMENTED
  - [x] Task: Full mode: send all chunks (fallback) — `CLOUD` — IMPLEMENTED
  - [x] Task: Session state tracked in DynamoDB (sentinel key timestamp=-1) — `CLOUD` — IMPLEMENTED
  - [x] Task: EventBridge retry timer (1-minute interval) — `CLOUD` — IMPLEMENTED
  - [x] Task: Stale session detection (30s threshold) — `CLOUD` — IMPLEMENTED
  - [x] Task: Max 5 retries before abort — `CLOUD` — IMPLEMENTED
  - [x] Task: CloudWatch alarms for Lambda errors and missing invocations — `CLOUD` — IMPLEMENTED

#### Feature 4.2.2: Deploy CLI
- **Story**: As an operator, I want a CLI tool to manage OTA deployments (baseline, deploy, preview, status, abort), so that firmware updates are predictable and controllable. **(S)**
  - [x] Task: `ota_deploy.py baseline/deploy/preview/status/abort` — `CLOUD` — IMPLEMENTED

### Milestone 4.3: OTA Pipeline (Device Side)

#### Feature 4.3.1: OTA State Machine
- **Story**: As a device, I want to receive firmware chunks, validate the image, and safely apply it with power-loss recovery, so that OTA updates never brick the device. **(S)**
  - [x] Task: OTA state machine: IDLE -> RECEIVING -> VALIDATING -> APPLYING -> reboot — `SW` — IMPLEMENTED
  - [x] Task: CRC32 validation on complete image — `SW` — IMPLEMENTED
  - [x] Task: Flash staging area at 0xD0000 (148KB) — `SW` — IMPLEMENTED
  - [x] Task: Delta mode: bitfield tracking of received chunks — `SW` — IMPLEMENTED
  - [x] Task: Delta mode: merged CRC validation (staging + primary) — `SW` — IMPLEMENTED
  - [x] Task: Deferred apply (15s delay for COMPLETE uplink to transmit) — `SW` — IMPLEMENTED
  - [x] Task: Recovery metadata at 0xCFF00 (survives power loss during apply) — `SW` — IMPLEMENTED
  - [x] Task: Boot recovery: resume interrupted flash copy — `SW` — IMPLEMENTED
  - [x] Task: Duplicate/stale START rejection (CRC match = already applied) — `SW` — IMPLEMENTED
  - [x] Task: Pre-apply hook: stop app callbacks before flash copy — `SW` — IMPLEMENTED

### Milestone 4.4: OTA Bug Fixes and Reliability

#### Feature 4.4.1: Stale Flash Data Fix (KI-003)
- **Story**: As an operator, I want OTA delta baselines to accurately reflect the running firmware, so that delta updates send the minimum number of chunks. **(M)**
  - [ ] Task: `flash.sh app` erases 0x90000-0xCEFFF before writing app hex — `SW` — NOT STARTED — TASK-022 (plan drafted, not approved)
  - [ ] Task: OTA apply (full, delta, recovery) erases pages beyond new image up to metadata boundary — `SW` — NOT STARTED
  - [ ] Task: `ota_deploy.py baseline` warns if dump is significantly larger than app.bin — `CLOUD` — NOT STARTED
  - [ ] Task: Host-side tests cover stale page erase after apply — `SW` — NOT STARTED
  - [ ] Task: Manual verification: flash large app -> flash small app -> dump partition -> stale bytes are 0xFF — `SW` — NOT STARTED

#### Feature 4.4.2: OTA Image Signing (Security)
- **Story**: As an operator, I want OTA images cryptographically signed with ED25519, so that a compromised S3 bucket cannot push malicious firmware. **(L)**
  - [ ] Task: Implement ED25519 signature generation in `ota_deploy.py` — `CLOUD` — NOT STARTED — PRD ref: TASK-031. Keys already in MFG store.
  - [ ] Task: Implement ED25519 signature verification on device before apply — `SW` — NOT STARTED
  - [ ] Task: Reject unsigned or badly-signed images with clear error — `SW` — NOT STARTED
  - [ ] Task: Document signing key management and rotation — `DOC` — NOT STARTED

---

## Epic 5: Smart Energy Layer

Time-of-use pricing, MOER/grid carbon, demand response, coordinated scheduling, and per-device utility configuration. The "gets smarter over time" promise. Covers PRD sections 4.4, 6.4.1.

### Milestone 5.1: Core Demand Response (Implemented)

#### Feature 5.1.1: TOU + MOER Scheduling
- **Story**: As a homeowner, I want my EV to charge automatically during cheap, clean-grid hours, so that I save money and reduce carbon emissions. **(S)**
  - [x] Task: Xcel Colorado TOU peak detection (weekdays 5-9 PM MT) — `CLOUD` — IMPLEMENTED
  - [x] Task: WattTime MOER grid signal for PSCO region — `CLOUD` — IMPLEMENTED
  - [x] Task: Decision: pause if TOU peak OR MOER > 70% threshold — `CLOUD` — IMPLEMENTED
  - [x] Task: MOER threshold validated (keep 70% for PSCO) — `CLOUD` — IMPLEMENTED (TASK-012)

### Milestone 5.2: Multi-Utility Support

#### Feature 5.2.1: Per-Device Utility Identification
- **Story**: As a system, I want to look up each device's utility and rate schedule, so that the charge scheduler applies the correct TOU windows for any utility territory. **(L)**
  - [ ] Task: Collect meter number during commissioning (installer enters it) — `DOC+CLOUD` — NOT STARTED — PRD ref: TASK-037
  - [ ] Task: Store meter number in device registry — `CLOUD` — NOT STARTED — Depends: Device Registry (Epic 9, Feature 9.2.1)
  - [ ] Task: Implement meter number -> utility -> TOU schedule lookup — `CLOUD` — NOT STARTED
  - [ ] Task: Update charge scheduler Lambda to use per-device utility config instead of hardcoded Xcel — `CLOUD` — NOT STARTED
  - [ ] Task: Handle multiple residential TOU plans within same utility (e.g., Xcel R, RE-TOU, S-EV) — `CLOUD` — NOT STARTED

### Milestone 5.3: Advanced Grid Signals

#### Feature 5.3.1: WattTime Historical Analysis
- **Story**: As an operator, I want to calibrate the MOER threshold using historical data, so that the threshold neither over-curtails nor under-curtails charging. **(M)**
  - [ ] Task: Upgrade WattTime to paid tier for historical data access — `CLOUD` — NOT STARTED (free tier only provides current value)
  - [ ] Task: Run full distribution analysis of MOER for PSCO region — `CLOUD` — NOT STARTED
  - [ ] Task: Simulate threshold comparison and recommend optimal threshold — `CLOUD` — NOT STARTED

#### Feature 5.3.2: Coordinated Fleet Scheduling
- **Story**: As a utility partner, I want SideCharge to shift both AC and EV loads in response to grid conditions, so that the distribution grid sees flexible demand from each installation. **(XL)**
  - [ ] Task: Design fleet-wide scheduling logic (stagger commands across devices) — `CLOUD` — NOT STARTED
  - [ ] Task: Implement command staggering (randomized 0-10 min delay per device) — `CLOUD` — NOT STARTED — Depends: Device Registry, Fleet management
  - [ ] Task: Implement rate limiting on cloud-to-device commands (max once per N minutes) — `SW+CLOUD` — NOT STARTED — PRD ref: TASK-030
  - [ ] Task: Device-side enforcement: ignore rapid command changes — `SW` — NOT STARTED

---

## Epic 6: Production Readiness

PCB design, 2-LED system, enclosure, BOM, manufacturing, 24VAC power supply, and UL certification path. The bridge from prototype to product.

### Milestone 6.1: Production PCB Design (TASK-019)

#### Feature 6.1.1: PCB Design and Fabrication
- **Story**: As a manufacturer, I want a production PCB with proper isolation, dual relay outputs, current transducer interface, and 24VAC power supply, so that we can produce devices beyond one-off prototypes. **(XL)**
  - [ ] Task: Design schematic — all interfaces from PRD 2.0.3 (4 inputs, 2 outputs, 1 power) — `HW` — NOT STARTED
  - [ ] Task: Include 24VAC to 3.3VDC AC-DC converter (replace USB power) — `HW` — NOT STARTED — PRD 6.1
  - [ ] Task: Include 2 LEDs (green + blue) per PRD 2.5.2 production LED matrix — `HW` — NOT STARTED
  - [ ] Task: Include "Charge Now" momentary push button — `HW` — NOT STARTED
  - [ ] Task: Include current clamp interface scaled to 48A minimum (evaluate 80A) — `HW` — NOT STARTED — Depends: Feature 2.2.1 scaling decision
  - [ ] Task: Include proper isolation barriers (optocouplers, magnetic couplers) — `HW` — NOT STARTED
  - [ ] Task: Route PCB layout — `HW` — NOT STARTED
  - [ ] Task: Generate BOM (bill of materials) — `HW+DOC` — NOT STARTED
  - [ ] Task: Prototype PCB fabrication and assembly — `HW` — NOT STARTED
  - [ ] Task: Verify all hardware interfaces on production PCB — `HW` — NOT STARTED

#### Feature 6.1.2: 24VAC Power Supply
- **Story**: As an installer, I want the device powered from the AC system's 24VAC transformer, so that no separate power supply or outlet is needed at the installation location. **(M)**
  - [ ] Task: Select and validate AC-DC converter module (24VAC -> 3.3VDC) — `HW` — NOT STARTED
  - [ ] Task: Verify power budget: RAK4631 + peripherals within 24VAC transformer capacity — `HW` — NOT STARTED
  - [ ] Task: Test power rail stability under LoRa TX current spikes — `HW` — NOT STARTED

### Milestone 6.2: LED System

#### Feature 6.2.1: Priority-Based Blink State Machine
- **Story**: As a homeowner, I want a single-glance LED indication of device health, so that I know everything is working without any interaction. **(M)**
  - [x] Task: LED control via app_leds module — `SW` — IMPLEMENTED
  - [ ] Task: Implement priority-based blink state machine (highest active state wins) — `SW` — NOT STARTED
  - [ ] Task: Implement prototype single-LED patterns per PRD 2.5.1 matrix (8 states) — `SW` — NOT STARTED
  - [ ] Task: Implement "Charge Now" button press acknowledgment (3 rapid blinks) — `SW` — NOT STARTED
  - [ ] Task: Implement commissioning mode auto-exit on first successful uplink — `SW` — NOT STARTED
  - [ ] Task: Implement error state entry criteria enforcement — `SW` — NOT STARTED
  - [ ] Task: Report LED state in uplink payload (cloud-side diagnostics) — `SW` — NOT STARTED

#### Feature 6.2.2: Production Dual-LED System
- **Story**: As an installer, I want two LEDs (green for health, blue for interlock), so that I can diagnose device status at a glance during commissioning. **(M)**
  - [ ] Task: Implement production dual-LED patterns per PRD 2.5.2 matrix — `SW` — NOT STARTED — Depends: TASK-019 PCB with 2 LEDs
  - [ ] Task: Implement combined reading patterns (green solid + blue solid = charging, etc.) — `SW` — NOT STARTED
  - [ ] Task: Implement dual-LED "Charge Now" button feedback (both LEDs flash 3 times) — `SW` — NOT STARTED

### Milestone 6.3: Enclosure and Physical Design

#### Feature 6.3.1: Device Enclosure
- **Story**: As an installer, I want a sealed enclosure that mounts near the junction box, so that the device is protected and tamper-evident. **(L)**
  - [ ] Task: Design enclosure (mountable below EV charger, access to wiring) — `HW` — NOT STARTED
  - [ ] Task: Include sealed USB port (no access post-factory) — `HW` — NOT STARTED — PRD 6.3.2
  - [ ] Task: Include LED light pipes / windows for green and blue LEDs — `HW` — NOT STARTED
  - [ ] Task: Include "Charge Now" button on enclosure exterior — `HW` — NOT STARTED
  - [ ] Task: Include device ID label (SC-XXXXXXXX) printed on enclosure — `HW+DOC` — NOT STARTED — PRD 4.6
  - [ ] Task: Tamper-evident sealing (noted in installation guide) — `HW+DOC` — DESIGNED

### Milestone 6.4: Manufacturing

#### Feature 6.4.1: Production BOM and Assembly
- **Story**: As a manufacturer, I want a complete BOM, assembly instructions, and test procedures, so that devices can be produced consistently beyond prototypes. **(XL)**
  - [ ] Task: Finalize BOM with component sourcing and costing — `HW+DOC` — NOT STARTED
  - [ ] Task: Develop factory assembly procedure — `DOC` — NOT STARTED
  - [ ] Task: Develop factory test procedure (verify all GPIOs, ADC channels, LoRa TX, LED function) — `DOC+SW` — NOT STARTED
  - [ ] Task: Develop factory provisioning workflow (MFG credentials -> platform -> app flash sequence) — `DOC` — NOT STARTED — Depends: TASK-011 provisioning docs
  - [ ] Task: Calculate target unit cost (goal: ~$1,000 equipment + installation) — `DOC` — NOT STARTED

---

## Epic 7: Code Compliance & Certification

NEC 220.60, NEC 220.70/Article 750, NEC 440.34, Colorado code, AHJ approval, inspector documentation, UL listing. The regulatory path to legal installation.

### Milestone 7.1: NEC Code Compliance

#### Feature 7.1.1: NEC 220.60 — Noncoincident Loads
- **Story**: As an electrician, I want to demonstrate to an inspector that the SideCharge interlock satisfies NEC 220.60, so that the installation passes code review. **(L)**
  - [ ] Task: Prepare formal documentation showing interlock prevents simultaneous operation — `DOC` — DESIGNED (not formally verified)
  - [ ] Task: Create load calculation worksheet showing only larger load used per 220.60 — `DOC` — NOT STARTED
  - [ ] Task: Obtain AHJ review of interlock design for NEC 220.60 compliance — `DOC` — NOT STARTED

#### Feature 7.1.2: NEC 220.70 / Article 750 — Energy Management Systems
- **Story**: As an electrician, I want documentation proving SideCharge qualifies as an energy management system under NEC 220.70/Article 750, so that the installation is code-compliant under the 2023 NEC. **(L)**
  - [ ] Task: Prepare compliance documentation for NEC 220.70 / Article 750 (automated load management for EV) — `DOC` — DESIGNED (not formally verified)
  - [ ] Task: Document dynamic load management capabilities (cloud override, TOU scheduling) — `DOC` — NOT STARTED
  - [ ] Task: Obtain AHJ review for Article 750 compliance — `DOC` — NOT STARTED

#### Feature 7.1.3: NEC 440.34 Exception — Interlocked AC Circuits
- **Story**: As an electrician, I want to use the NEC 440.34 exception to reduce conductor and overcurrent protection sizing for the interlocked AC circuit, so that installation costs are minimized. **(M)**
  - [ ] Task: Prepare compliance documentation for NEC 440.34 exception — `DOC` — DESIGNED (not formally verified)
  - [ ] Task: Create conductor sizing table for typical installation scenarios (40A, 50A circuits) — `DOC` — NOT STARTED

### Milestone 7.2: Colorado Code Compliance

#### Feature 7.2.1: State-Specific Requirements
- **Story**: As an installer operating in Colorado, I want confirmation that SideCharge installations comply with Colorado electrical code amendments, so that permits are approved. **(M)**
  - [ ] Task: Review Colorado-specific amendments to NEC for EV charger installations — `DOC` — NOT STARTED — DESIGNED (not formally verified)
  - [ ] Task: Verify compatibility with local AHJ requirements in target service areas — `DOC` — NOT STARTED

### Milestone 7.3: UL Listing and Safety Certification

#### Feature 7.3.1: UL Certification Path
- **Story**: As a company, I want to obtain UL listing (or equivalent NRTL certification), so that the product can legally be sold and installed through the electrician channel. **(XL)**
  - [ ] Task: Identify applicable UL standard(s) for low-voltage control device — `DOC` — NOT STARTED — PRD notes: because device stays in low-voltage domain, certification path should be simpler than mains-switching devices
  - [ ] Task: Engage NRTL (Nationally Recognized Testing Laboratory) — `DOC` — NOT STARTED
  - [ ] Task: Prepare product samples and documentation for UL evaluation — `HW+DOC` — NOT STARTED — Depends: Production PCB (Milestone 6.1)
  - [ ] Task: Budget and timeline for UL certification process — `DOC` — NOT STARTED
  - [ ] Task: Address any UL findings and iterate on design — `HW` — NOT STARTED

#### Feature 7.3.2: Heat Pump Compatibility (Future)
- **Story**: As a product, I want to be compatible with heat pump systems (reversing valve, defrost cycle), so that the addressable market expands beyond AC-only homes. **(L)**
  - [ ] Task: Research heat pump interlock requirements (reversing valve, defrost cycle timing) — `DOC+HW` — NOT STARTED — PRD 2.0.5: Future goal, not v1.0 scope
  - [ ] Task: Design heat call interlock logic (heat pumps use same compressor for heating and cooling) — `SW` — NOT STARTED

---

## Epic 8: Installer Experience

Commissioning sequence, LED patterns, installation guide, wiring diagrams, printed checklist, electrician training materials. The installer is the primary buyer.

### Milestone 8.1: Commissioning Process

#### Feature 8.1.1: LED-Based Commissioning Sequence
- **Story**: As an installer, I want a clear LED sequence that tells me the device is wired correctly, connected to Sidewalk, and the interlock is working, so that I can verify the installation in under 5 minutes. **(M)**
  - [ ] Task: Implement commissioning mode — 1Hz flash until first successful Sidewalk uplink (or 5 min timeout) — `SW` — NOT STARTED — Depends: Feature 6.2.1 (LED state machine)
  - [ ] Task: Document commissioning sequence steps for installer (PRD 2.5.1 has the 6-step sequence) — `DOC` — NOT STARTED
  - [ ] Task: Define commissioning failure modes and installer actions (LED stays on 1Hz > 5 min = check gateway proximity) — `DOC` — NOT STARTED

#### Feature 8.1.2: Production Commissioning Process
- **Story**: As an installer, I want a defined commissioning process that does not require a phone app or cloud access, so that I can verify the installation with just the device and a printed checklist. **(M)**
  - [ ] Task: Define production commissioning process (LED-based, no phone/cloud required) — `DOC` — NOT STARTED — PRD ref: TASK-024 in known gaps (PM decision needed)
  - [ ] Task: Define commissioning data collection (device ID, installer name, install address, meter number) — `DOC` — NOT STARTED
  - [ ] Task: Decide how commissioning data enters the cloud (manual entry? photo of label? phone tool?) — `DOC` — TBD

### Milestone 8.2: Installation Documentation

#### Feature 8.2.1: Installation Guide
- **Story**: As an installer, I want a printed installation guide with step-by-step instructions and wiring diagrams, so that any licensed electrician can install SideCharge correctly. **(L)**
  - [ ] Task: Write step-by-step installation guide — `DOC` — NOT STARTED — PRD 1.4: deliverable of TASK-019 PCB design
  - [ ] Task: Create wiring diagrams for typical installations (40A circuit, 50A circuit) — `DOC+HW` — NOT STARTED
  - [ ] Task: Create terminal specification sheet — `DOC+HW` — NOT STARTED
  - [ ] Task: Document branch circuit modification procedure (junction box, conductor runs, conduit) — `DOC` — NOT STARTED
  - [ ] Task: Document NEC compliance requirements installer must verify (conductor gauge, breaker rating, etc.) — `DOC` — NOT STARTED

#### Feature 8.2.2: Printed Commissioning Checklist
- **Story**: As an installer, I want a printed checklist card included in the box, so that I can verify each step of the installation without consulting a manual. **(S)**
  - [ ] Task: Design printed commissioning checklist card — `DOC` — NOT STARTED — PRD 1.4: "A printed commissioning checklist card is included in the box"
  - [ ] Task: Include: power verification, interlock response, connectivity, interlock under load — `DOC` — NOT STARTED

### Milestone 8.3: Training Materials

#### Feature 8.3.1: Electrician Training
- **Story**: As a company, I want training materials that explain SideCharge to electricians, so that they can sell and install the product confidently. **(L)**
  - [ ] Task: Create electrician training deck (what it does, how it works, NEC compliance, installation steps) — `DOC` — NOT STARTED
  - [ ] Task: Create FAQ document for common installer questions — `DOC` — NOT STARTED
  - [ ] Task: Create troubleshooting guide (LED patterns, common wiring mistakes, gateway issues) — `DOC` — NOT STARTED

---

## Epic 9: Cloud Dashboard & Monitoring

Operator dashboard, device fleet management, OTA deployment pipeline, alerting, analytics. Covers PRD sections 4.6, 5.3.

### Milestone 9.1: Production Observability (Tier 1 — Cloud Only)

#### Feature 9.1.1: Device Offline Detection
- **Story**: As an operator, I want to be alerted when a device stops reporting, so that silent failures are detected within hours, not days. **(S)**
  - [ ] Task: CloudWatch metric filter on DynamoDB writes per device_id — `CLOUD` — NOT STARTED — PRD 5.3.3 Tier 1
  - [ ] Task: Alarm when no write for 2x heartbeat interval — `CLOUD` — NOT STARTED
  - [ ] Task: SNS notification to operator — `CLOUD` — NOT STARTED

#### Feature 9.1.2: OTA Failure Alerting
- **Story**: As an operator, I want to be notified when an OTA update stalls or aborts, so that I can intervene before the device is stuck in a bad state. **(S)**
  - [x] Task: CloudWatch alarms for OTA sender Lambda errors — `CLOUD` — IMPLEMENTED (partially)
  - [ ] Task: Alarm on stalled OTA sessions (no ACK for >5 min) — `CLOUD` — NOT STARTED
  - [ ] Task: SNS notification with device ID and failure details — `CLOUD` — NOT STARTED

#### Feature 9.1.3: Interlock State Change Logging
- **Story**: As an operator, I want a dashboard showing interlock activations per day per device, so that I can verify the system is working and generate compliance reports. **(M)**
  - [ ] Task: Decode Lambda extracts cool_call transitions from uplink payload — `CLOUD` — NOT STARTED
  - [ ] Task: CloudWatch metric filter for interlock state transitions — `CLOUD` — NOT STARTED
  - [ ] Task: Dashboard widget showing interlock activations per day — `CLOUD` — NOT STARTED

#### Feature 9.1.4: Daily Health Digest
- **Story**: As an operator, I want a daily email summary of fleet health (devices online, firmware versions, error counts), so that I have a single daily checkpoint. **(M)**
  - [ ] Task: Implement scheduled Lambda that queries DynamoDB for all devices — `CLOUD` — NOT STARTED
  - [ ] Task: Check last-seen timestamp, firmware version, error counts per device — `CLOUD` — NOT STARTED
  - [ ] Task: Send summary email via SNS/SES — `CLOUD` — NOT STARTED

### Milestone 9.2: Device Registry and Fleet Management

#### Feature 9.2.1: Device Registry
- **Story**: As an operator, I want a persistent registry of all devices with owner, location, firmware version, and liveness, so that I can manage the fleet and support customers. **(L)**
  - [ ] Task: Create DynamoDB `sidecharge-device-registry` table (Terraform-managed) — `CLOUD` — NOT STARTED — PRD ref: TASK-036
  - [ ] Task: Implement device ID generation: `SC-` + first 8 hex chars of SHA-256(sidewalk_uuid) — `CLOUD` — DESIGNED
  - [ ] Task: Create GSI on `owner_email` for "my devices" lookup — `CLOUD` — NOT STARTED
  - [ ] Task: Create GSI on `status` for fleet health queries — `CLOUD` — NOT STARTED
  - [ ] Task: Implement status lifecycle: provisioned -> installed -> active -> inactive / returned — `CLOUD` — DESIGNED
  - [ ] Task: Populate registry at factory provisioning (device_id, sidewalk_id, provisioned_date) — `CLOUD` — NOT STARTED
  - [ ] Task: Populate registry at installation (owner, address, installer, meter number, install_date) — `CLOUD` — NOT STARTED
  - [ ] Task: Decode Lambda updates `last_seen` and `app_version` on every uplink — `CLOUD` — NOT STARTED

### Milestone 9.3: Remote Diagnostics (Tier 2)

#### Feature 9.3.1: Remote Status Query
- **Story**: As an operator, I want to remotely request an on-demand status report from a device, so that I can diagnose issues without waiting for the next heartbeat. **(M)**
  - [ ] Task: Implement new downlink command (0x40?) that triggers immediate uplink with extended diagnostics — `SW+CLOUD` — NOT STARTED — PRD 5.3.3 Tier 2
  - [ ] Task: Implement extended diagnostics payload (0xE6 magic?): firmware version, uptime, Sidewalk state, last error, interlock state, Charge Now active, boot count — `SW` — NOT STARTED
  - [ ] Task: CLI command for operator to trigger remote status query — `CLOUD` — NOT STARTED

### Milestone 9.4: BLE Diagnostics (Tier 3)

#### Feature 9.4.1: BLE Diagnostics Beacon
- **Story**: As an installer doing field service, I want to read device diagnostics via BLE on my phone, so that I can diagnose a device that is not connecting to the cloud. **(M)**
  - [ ] Task: On boot, advertise BLE beacon for 5 minutes with device ID, firmware version, Sidewalk state, interlock state, last error — `SW` — NOT STARTED — PRD 5.3.3 Tier 3
  - [ ] Task: Long-press "Charge Now" button (10 seconds) activates BLE beacon for 5 minutes — `SW` — NOT STARTED — Depends: Feature 1.2.2 (button handler)
  - [ ] Task: BLE auto-disable after timeout (prevent persistent BLE attack surface) — `SW` — NOT STARTED

---

## Epic 10: Testing & Validation

Unit tests, integration tests, field testing with real J1772 hardware, load testing, security testing, OTA reliability testing. Covers PRD section 5.4.

### Milestone 10.1: Host-Side Unit Tests (C Firmware)

#### Feature 10.1.1: App Layer Tests
- **Story**: As a developer, I want comprehensive host-side unit tests for all app-layer logic, so that I can catch regressions without hardware. **(S)**
  - [x] Task: Unity/CMake test framework setup — `SW` — IMPLEMENTED (90+ tests)
  - [x] Task: J1772 state machine tests (voltage thresholds, state classification) — `SW` — IMPLEMENTED
  - [x] Task: Charge control tests (allow/pause, auto-resume) — `SW` — IMPLEMENTED
  - [x] Task: Thermostat input tests — `SW` — IMPLEMENTED
  - [x] Task: Payload encoding tests (uplink format) — `SW` — IMPLEMENTED
  - [x] Task: Downlink parsing tests — `SW` — IMPLEMENTED
  - [x] Task: Shell command dispatch tests — 31 tests — `SW` — IMPLEMENTED (TASK-027)
  - [x] Task: MFG key health check tests — 7 tests — `SW` — IMPLEMENTED (TASK-028)

#### Feature 10.1.2: Platform Layer Tests
- **Story**: As a developer, I want host-side tests for the OTA state machine, chunk receive, delta bitmap, and flash recovery, so that safety-critical OTA code is verified. **(S)**
  - [x] Task: OTA recovery path tests — 16 tests (mock flash, power-loss scenarios) — `SW` — IMPLEMENTED (TASK-005)
  - [x] Task: OTA chunk receive and delta bitmap tests — 13 tests — `SW` — IMPLEMENTED (TASK-025)

#### Feature 10.1.3: Boot Path Tests
- **Story**: As a developer, I want tests for `discover_app_image()` and the boot path, so that version mismatch, bad magic, and NULL app_cb are verified. **(M)**
  - [ ] Task: Test: valid magic + version -> app callbacks invoked — `SW` — NOT STARTED — TASK-026
  - [ ] Task: Test: wrong magic -> app not loaded, platform boots standalone — `SW` — NOT STARTED
  - [ ] Task: Test: wrong version -> app not loaded (hard stop per ADR-001) — `SW` — NOT STARTED
  - [ ] Task: Test: OTA message (cmd 0x20) routed to OTA engine, not app — `SW` — NOT STARTED
  - [ ] Task: Test: non-OTA message routed to app_cb->on_msg_received — `SW` — NOT STARTED
  - [ ] Task: Test: app_cb NULL -> messages handled safely (no crash) — `SW` — NOT STARTED
  - [ ] Task: Test: timer interval bounds (< 100ms rejected, > 300000ms rejected) — `SW` — NOT STARTED

#### Feature 10.1.4: Grenning Legacy Tests
- **Story**: As a developer, I want the original 32 Grenning dual-target tests maintained in CI, so that no regressions slip through during framework migration. **(S)**
  - [x] Task: Grenning test framework (32 tests, Makefile-based) — `SW` — IMPLEMENTED
  - [x] Task: Grenning tests added to CI — `SW` — IMPLEMENTED (TASK-018)

### Milestone 10.2: Python Lambda Tests

#### Feature 10.2.1: Lambda Test Coverage
- **Story**: As a developer, I want comprehensive pytest coverage of all Lambda functions, so that cloud logic is verified before deployment. **(S)**
  - [x] Task: Decode Lambda tests — 18 tests — `CLOUD` — IMPLEMENTED (TASK-006)
  - [x] Task: Charge scheduler Lambda tests — 19 tests — `CLOUD` — IMPLEMENTED (TASK-004)
  - [x] Task: OTA sender Lambda tests — 27 tests — `CLOUD` — IMPLEMENTED
  - [x] Task: OTA deploy CLI tests — 16 tests — `CLOUD` — IMPLEMENTED
  - [x] Task: Lambda chain integration tests — 14 tests — `CLOUD` — IMPLEMENTED
  - [x] Task: Python linting (ruff) in CI — `CLOUD` — IMPLEMENTED (TASK-010)

### Milestone 10.3: Integration and E2E Tests

#### Feature 10.3.1: Serial Integration Tests
- **Story**: As a developer, I want to run automated tests against a physical device via serial, so that shell commands and state simulation are verified on real hardware. **(S)**
  - [x] Task: Serial integration test framework (pytest + serial) — 7 tests — `SW` — IMPLEMENTED
  - [x] Task: E2E runbook written (`tests/e2e/RUNBOOK.md`) — `DOC` — IMPLEMENTED (TASK-007)
  - [x] Task: E2E runbook executed — 6/7 pass (OTA skipped) — `SW` — IMPLEMENTED (TASK-020)

#### Feature 10.3.2: Device-to-Cloud Round-Trip Tests
- **Story**: As a developer, I want automated end-to-end tests covering the full uplink/downlink/OTA path, so that the entire system is validated as a unit. **(L)**
  - [ ] Task: Automate E2E uplink path test (device -> Sidewalk -> decode Lambda -> DynamoDB) — `SW+CLOUD` — NOT STARTED
  - [ ] Task: Automate E2E downlink path test (charge control -> IoT Wireless -> device) — `SW+CLOUD` — NOT STARTED
  - [ ] Task: Automate E2E OTA path test (deploy -> status -> complete -> reboot) — `SW+CLOUD` — NOT STARTED
  - [ ] Task: Automate recovery path test (power cycle during OTA apply) — `SW` — NOT STARTED

### Milestone 10.4: Field Testing

#### Feature 10.4.1: OTA Field Reliability
- **Story**: As an operator, I want OTA updates tested at various RF conditions (lab, indoor 10m, outdoor 50m, max range 200m+), so that I know the reliability envelope before deploying to customer homes. **(L)**
  - [x] Task: Write OTA field test plan — 4 conditions, 3 cycles each, go/no-go framework — `DOC` — IMPLEMENTED (TASK-013, partial)
  - [ ] Task: Execute OTA field test — lab (~1m) — `HW` — NOT STARTED (requires physical field work)
  - [ ] Task: Execute OTA field test — indoor (~10m) — `HW` — NOT STARTED
  - [ ] Task: Execute OTA field test — outdoor (~50m) — `HW` — NOT STARTED
  - [ ] Task: Execute OTA field test — max range (~200m+) — `HW` — NOT STARTED
  - [ ] Task: Test power-cycle-during-apply in field conditions — `HW` — NOT STARTED
  - [ ] Task: Document results and go/no-go recommendation — `DOC` — NOT STARTED

#### Feature 10.4.2: J1772 Hardware Validation
- **Story**: As an operator, I want the J1772 pilot monitoring tested with real physical chargers and vehicles, so that state classification is reliable in production. **(L)**
  - [ ] Task: Test with at least 3 different Level 2 J1772 chargers — `HW` — NOT STARTED — PRD 2.0.5: "tested with simulated signals only"
  - [ ] Task: Test with at least 3 different EV makes (verify pause/resume behavior) — `HW` — NOT STARTED — PRD ref: Oliver EXP-001
  - [ ] Task: Validate current clamp readings against known reference meter — `HW` — NOT STARTED
  - [ ] Task: Document charger compatibility matrix — `DOC` — NOT STARTED

#### Feature 10.4.3: Interlock Hardware Integration Tests
- **Story**: As a safety engineer, I want the complete HW+SW interlock tested with real thermostat and charger hardware, so that mutual exclusion is verified under real conditions. **(L)**
  - [ ] Task: Test interlock with real thermostat (Nest, Ecobee, Honeywell) — `HW` — NOT STARTED
  - [ ] Task: Test AC priority: real thermostat call pauses real EV charger — `HW` — NOT STARTED
  - [ ] Task: Test EV lockout: real current flowing blocks real thermostat signal — `HW` — NOT STARTED
  - [ ] Task: Test "Charge Now" button with real loads — `HW` — NOT STARTED
  - [ ] Task: Test fail-safe: remove MCU power, verify HW interlock holds — `HW` — NOT STARTED
  - [ ] Task: Test power cycle during various interlock states — `HW` — NOT STARTED

### Milestone 10.5: Security Testing

#### Feature 10.5.1: Security Validation
- **Story**: As a security engineer, I want the threat model validated through testing, so that production deployment does not introduce unacceptable risks. **(L)**
  - [ ] Task: Test: double-load scenario (both loads enabled) — verify breaker trips and HW interlock prevents — `HW` — NOT STARTED
  - [ ] Task: Test: rapid cloud command injection — verify device rate-limiting — `SW+CLOUD` — NOT STARTED — Depends: Rate limiting implementation (Feature 5.3.2)
  - [ ] Task: Test: malformed downlink handling — verify graceful rejection — `SW` — NOT STARTED
  - [ ] Task: Test: OTA with tampered image (bad CRC, bad signature) — verify rejection — `SW` — NOT STARTED — Depends: OTA signing (Feature 4.4.2)
  - [ ] Task: Verify no PII in CloudWatch logs — `CLOUD` — NOT STARTED — PRD 6.4.2: NOT VERIFIED
  - [ ] Task: Review IAM permissions for least privilege — `CLOUD` — NOT STARTED

### Milestone 10.6: CI/CD Pipeline

#### Feature 10.6.1: Continuous Integration
- **Story**: As a developer, I want all tests to run automatically on push and PR, so that regressions are caught before merge. **(S)**
  - [x] Task: GitHub Actions workflow (`.github/workflows/ci.yml`) — `CLOUD` — IMPLEMENTED (TASK-009)
  - [x] Task: cppcheck static analysis in CI — `SW` — IMPLEMENTED
  - [x] Task: CMake/ctest C unit tests in CI — `SW` — IMPLEMENTED
  - [x] Task: Grenning Makefile C tests in CI — `SW` — IMPLEMENTED (TASK-018)
  - [x] Task: Python pytest in CI — `CLOUD` — IMPLEMENTED (TASK-010)
  - [x] Task: Python ruff linting in CI — `CLOUD` — IMPLEMENTED

---

## Epic 11: Privacy, Compliance, and Documentation

Data privacy, retention policies, CCPA compliance, and operator documentation. Covers PRD sections 6.4, 5.1, 5.2.

### Milestone 11.1: Data Privacy

#### Feature 11.1.1: Privacy Policy and Retention
- **Story**: As a company, I want a data privacy policy and retention rules, so that customer data is handled responsibly and we comply with state privacy laws. **(M)**
  - [ ] Task: Define data retention policy (how long raw telemetry is kept vs. aggregated stats) — `DOC` — NOT STARTED — PRD ref: TASK-038
  - [ ] Task: Write customer-facing privacy policy document — `DOC` — NOT STARTED
  - [ ] Task: Implement DynamoDB TTL for raw telemetry records (per retention policy) — `CLOUD` — NOT STARTED
  - [ ] Task: Implement customer data deletion on device return/decommission — `CLOUD` — NOT STARTED
  - [ ] Task: CCPA/state privacy law compliance review — `DOC` — NOT STARTED
  - [ ] Task: Verify no PII in CloudWatch logs — `CLOUD` — NOT STARTED — PRD 6.4.2: NOT VERIFIED

### Milestone 11.2: Operator Documentation

#### Feature 11.2.1: OTA Recovery Runbook
- **Story**: As an operator, I want a runbook for diagnosing and recovering from OTA failures, so that I do not need tribal knowledge to fix a stuck device. **(S)**
  - [x] Task: OTA recovery runbook — 533-line document at `docs/ota-recovery.md` — `DOC` — IMPLEMENTED (TASK-008)
  - [x] Task: OTA state machine diagram — `DOC` — IMPLEMENTED
  - [x] Task: Failure modes documented (power loss, CRC mismatch, magic failure, stale flash) — `DOC` — IMPLEMENTED
  - [x] Task: Manual recovery procedures — `DOC` — IMPLEMENTED
  - [x] Task: Full `ota_deploy.py` CLI reference — `DOC` — IMPLEMENTED

#### Feature 11.2.2: Provisioning Documentation
- **Story**: As a developer, I want step-by-step provisioning documentation, so that anyone can set up a new device without tribal knowledge. **(S)**
  - [x] Task: Provisioning guide at `docs/provisioning.md` — `DOC` — IMPLEMENTED (TASK-011)
  - [x] Task: Credential generation, MFG partition build, flash sequence, first boot — `DOC` — IMPLEMENTED
  - [x] Task: AWS IoT Wireless registration — `DOC` — IMPLEMENTED
  - [x] Task: E2E verification steps — `DOC` — IMPLEMENTED

#### Feature 11.2.3: Architecture Documentation
- **Story**: As a developer, I want architecture documentation covering the split-image design, API contract, and SDK compliance, so that new contributors can understand the system. **(S)**
  - [x] Task: Architecture document at `docs/architecture.md` — `DOC` — IMPLEMENTED (TASK-016)
  - [x] Task: Memory layout diagram — `DOC` — IMPLEMENTED
  - [x] Task: Platform API contract (22 functions, 7 callbacks) — `DOC` — IMPLEMENTED
  - [x] Task: SDK compliance table — `DOC` — IMPLEMENTED
  - [x] Task: OTA flow diagram — `DOC` — IMPLEMENTED
  - [x] Task: Testing architecture — `DOC` — IMPLEMENTED

#### Feature 11.2.4: Cloud Command Authentication (Security Doc)
- **Story**: As a security engineer, I want downlink commands to be signed (not just encrypted in transit), so that a compromised network cannot inject commands. **(L)**
  - [ ] Task: Design command authentication scheme (signed downlinks) — `DOC+SW+CLOUD` — NOT STARTED — PRD ref: TASK-032
  - [ ] Task: Implement cloud-side command signing — `CLOUD` — NOT STARTED
  - [ ] Task: Implement device-side signature verification on downlinks — `SW` — NOT STARTED
  - [ ] Task: Document key management for command signing — `DOC` — NOT STARTED

---

## Cross-Epic Dependencies

The following dependencies span multiple epics and must be tracked carefully:

| Dependency | From | To | Notes |
|------------|------|----|-------|
| Production PCB (TASK-019) | Epic 6 Milestone 6.1 | Epic 6 Feature 6.2.2, Epic 8 Feature 8.2.1 | Dual LEDs, enclosure, wiring diagrams all depend on PCB |
| Device Registry | Epic 9 Feature 9.2.1 | Epic 5 Feature 5.2.1, Epic 3 Feature 3.4.1 | Per-device utility config and decode Lambda updates depend on registry |
| LED State Machine | Epic 6 Feature 6.2.1 | Epic 8 Feature 8.1.1 | Commissioning sequence depends on LED patterns |
| "Charge Now" Button | Epic 1 Feature 1.2.2 | Epic 9 Feature 9.4.1 | BLE diagnostics activation depends on button handler |
| TIME_SYNC Downlink | Epic 3 Feature 3.3.1 | Epic 3 Feature 3.2.2 | Event buffer ACK trimming depends on TIME_SYNC |
| OTA Signing | Epic 4 Feature 4.4.2 | Epic 10 Feature 10.5.1 | Security testing of OTA depends on signing implementation |
| Rate Limiting | Epic 5 Feature 5.3.2 | Epic 10 Feature 10.5.1 | Security testing of command injection depends on rate limiting |
| Current Clamp Scaling | Epic 2 Feature 2.2.1 | Epic 6 Feature 6.1.1 | PCB clamp interface depends on scaling decision (48A vs 80A) |
| Stale Flash Fix (KI-003) | Epic 4 Feature 4.4.1 | All OTA operations | Delta OTA reliability depends on clean baselines |
| Boot Path Tests (TASK-026) | Epic 10 Feature 10.1.3 | Blocked by TASK-024 (done) | Version mismatch behavior defined, tests can proceed |
| Merge to Main (TASK-001) | All epics | All new work should be based on merged `main` | `feature/generic-platform` and `feature/testing-pyramid` need merge |

---

## Priority Phases

### Phase 0: Merge and Stabilize (Current Sprint)
**Goal**: Get all completed work onto `main`, fix the one open bug.
1. TASK-001: Merge `feature/generic-platform` to main — **P1**
2. Merge `feature/testing-pyramid` to main — **P1**
3. TASK-022: Fix stale flash data (KI-003) — **P1**
4. TASK-026: Boot path tests — **P2**

### Phase 1: Firmware Completeness (Weeks 1-3)
**Goal**: All firmware features designed in the PRD are implemented.
1. Feature 1.2.1: Boot/power recovery behavior (read thermostat before charge enable)
2. Feature 1.2.2: "Charge Now" override button
3. Feature 6.2.1: Priority-based LED state machine (prototype single-LED)
4. Feature 3.2.1: Expand uplink payload to 12 bytes with timestamp
5. Feature 3.3.1: TIME_SYNC downlink command
6. Feature 3.2.2: Device-side event buffer (ring buffer)
7. Feature 2.1.2: J1772 PWM duty cycle decoding
8. Feature 1.2.4: Interlock transition logging

### Phase 2: Cloud Completeness (Weeks 3-5)
**Goal**: Cloud infrastructure supports production monitoring and fleet management.
1. Feature 9.2.1: Device registry (DynamoDB table, Terraform)
2. Feature 9.1.1: Device offline detection (CloudWatch alarms)
3. Feature 9.1.2: OTA failure alerting
4. Feature 9.1.3: Interlock state change logging
5. Feature 9.1.4: Daily health digest
6. Feature 3.4.1: Update decode Lambda for v0x07 payload

### Phase 3: Field Validation (Weeks 5-8)
**Goal**: System validated with real hardware, real LoRa conditions, real J1772 equipment.
1. Feature 10.4.1: OTA field reliability testing
2. Feature 10.4.2: J1772 hardware validation (3+ chargers, 3+ car makes)
3. Feature 10.4.3: Interlock hardware integration tests
4. Feature 2.1.3: Field calibration of J1772 thresholds
5. Feature 2.2.1: Current clamp rescaling (48A minimum)

### Phase 4: Production Hardware (Weeks 8-16)
**Goal**: Production PCB designed, fabricated, and verified.
1. Feature 6.1.1: PCB design and fabrication
2. Feature 6.1.2: 24VAC power supply
3. Feature 6.2.2: Production dual-LED system
4. Feature 6.3.1: Device enclosure
5. Feature 6.4.1: Production BOM and assembly

### Phase 5: Code Compliance and Certification (Weeks 12-24)
**Goal**: NEC compliance documented, AHJ reviewed, UL path started.
1. Feature 7.1.1: NEC 220.60 compliance documentation
2. Feature 7.1.2: NEC 220.70 / Article 750 compliance documentation
3. Feature 7.1.3: NEC 440.34 exception documentation
4. Feature 7.2.1: Colorado code compliance review
5. Feature 7.3.1: UL certification engagement (long lead time)

### Phase 6: Installer Experience and Launch Prep (Weeks 16-24)
**Goal**: Installation documentation, training materials, and commissioning process ready.
1. Feature 8.2.1: Installation guide with wiring diagrams
2. Feature 8.2.2: Printed commissioning checklist
3. Feature 8.1.1: LED-based commissioning sequence
4. Feature 8.1.2: Production commissioning process
5. Feature 8.3.1: Electrician training materials
6. Feature 11.1.1: Privacy policy and data retention

### Phase 7: Security Hardening (Weeks 18-24)
**Goal**: Security gaps closed before any customer deployment.
1. Feature 4.4.2: OTA image ED25519 signing
2. Feature 11.2.4: Cloud command authentication (signed downlinks)
3. Feature 5.3.2: Fleet-wide command throttling and rate limiting
4. Feature 10.5.1: Security validation testing

---

## Summary Statistics

| Metric | Count |
|--------|-------|
| Epics | 11 |
| Milestones | 34 |
| Features | 62 |
| Total tasks | ~220 |
| Tasks IMPLEMENTED (checked) | ~95 |
| Tasks remaining (unchecked) | ~125 |
| Known issues open | 1 (KI-003: stale flash) |
| Known issues resolved | 2 (KI-001: documented workaround, KI-002: mitigated) |

The foundation is solid: the interlock hardware works, Sidewalk connectivity works, OTA pipeline works, demand response works, and CI runs 200+ tests. The path to production requires completing the firmware gaps (boot behavior, LED system, timestamps), building the production hardware (PCB, 24VAC, enclosure), validating with real equipment (J1772 chargers, thermostats, field RF conditions), navigating code compliance (NEC, UL), and building the installer experience (documentation, training, commissioning).

The single biggest non-technical risk remains UL listing: no timeline, no budget, no NRTL engagement. This should be the next PM decision after Phase 0 stabilization.
