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

### 4.5 Utility Identification and Multi-Utility Support

#### 4.5.1 Problem

The charge scheduler (section 4.4) hardcodes Xcel Colorado: `WATTTIME_REGION = "PSCO"`, `is_tou_peak()` checks weekdays 5-9 PM Mountain Time. This works for the single-site v1.0 deployment but blocks multi-customer or multi-region use. Any second customer on a different utility — or even a different Xcel rate plan — gets the wrong schedule.

#### 4.5.2 Lookup Pipeline

Utility identification requires a two-step lookup, not one. The original assumption (meter number → utility → TOU) is wrong because US electric meter numbers are utility-specific with no standard format — you cannot determine the utility from the meter number alone.

The correct pipeline is:

```
install_address → utility → TOU schedule
                          → WattTime region
meter_number   → rate plan (within the utility)
```

**Step 1: Address → Utility.** The install address (collected during commissioning, stored in the device registry) identifies which utility serves the property. In v1.0 this is manually configured. In v1.1+ it could be automated via the OpenEI USURDB API (free, from NREL — 3,700+ US utilities, lookup by address or zip code, requires free API key from developer.nrel.gov).

**Step 2: Utility → TOU Schedule + WattTime Region.** Each utility maps to a TOU schedule (peak window, timezone, weekend rules) and a WattTime balancing authority region. This is a static configuration table — utilities change rate structures at most once or twice per year.

**Step 3: Meter Number → Rate Plan (optional).** Within a single utility, the meter number disambiguates the customer's rate plan. Xcel Colorado has multiple residential TOU plans (R, RE-TOU, S-EV) with different peak windows and pricing. For v1.0, we assume all Xcel customers are on the standard TOU plan. For v1.1+, the meter number (or customer self-selection) resolves the specific plan.

#### 4.5.3 TOU Schedule Data Model

Each TOU schedule is a JSON object describing the peak window:

```json
{
  "schedule_id": "xcel-co-tou-residential",
  "utility_name": "Xcel Energy",
  "utility_region": "Colorado",
  "timezone": "America/Denver",
  "watttime_region": "PSCO",
  "peak_windows": [
    {
      "name": "weekday_evening",
      "days": [0, 1, 2, 3, 4],
      "start_hour": 17,
      "end_hour": 21,
      "seasonal": false
    }
  ],
  "notes": "Weekdays 5-9 PM MT year-round. Weekends and holidays are off-peak."
}
```

The `peak_windows` array supports multiple windows (e.g., summer vs. winter) and seasonal variation. The `days` array uses Python weekday convention (0=Monday, 6=Sunday).

Reference schedules for the top 5 US residential EV utility markets:

| Utility | Region | TOU Peak | Timezone | WattTime BA | Notes |
|---------|--------|----------|----------|-------------|-------|
| Xcel Energy | Colorado (PSCO) | Weekdays 5-9 PM | America/Denver | PSCO | v1.0 target. Year-round, no seasonal variation. |
| SCE | Southern California | Weekdays 4-9 PM | America/Los_Angeles | CAISO | Some plans 5-8 PM. Summer/winter rates differ. |
| PG&E | Northern California | Weekdays 4-9 PM | America/Los_Angeles | CAISO | EV-specific plans with super off-peak midnight-3 PM. |
| SDG&E | San Diego | Weekdays 4-9 PM | America/Los_Angeles | CAISO | Similar to SCE/PG&E. |
| Con Edison | New York | Weekdays 2-6 PM (summer only, Jun-Sep) | America/New_York | NYISO | Seasonal — no TOU peak in winter. |

**Key observation**: Peak windows are remarkably consistent — 4-9 PM is the de facto standard for California utilities, Xcel is 5-9 PM, and Con Ed is the outlier at 2-6 PM summer-only. A static table covering these 5 utilities handles the vast majority of US residential EV charging deployments.

#### 4.5.4 Configuration Storage

**v1.0**: No per-device config. Hardcoded Xcel Colorado in the Lambda. One utility, one schedule.

**v1.1**: TOU schedule table in DynamoDB (`sidecharge-tou-schedules`), keyed by `schedule_id`. Device registry (TASK-036) gains a `schedule_id` field that maps each device to its TOU schedule. The charge scheduler Lambda reads the device's `schedule_id` from the registry and loads the corresponding schedule. Devices without a `schedule_id` fall back to Xcel Colorado (backward-compatible default).

**v1.1+ (optional)**: OpenEI API integration — on commissioning, automatically look up the utility from the install address and assign the default TOU schedule for that utility. Installer confirms or overrides.

#### 4.5.5 Charge Scheduler Refactor Path

The current `charge_scheduler_lambda.py` needs these changes for multi-utility support:

1. **`is_tou_peak(now_mt)`** → **`is_tou_peak(now, schedule)`** — Accepts a schedule object instead of hardcoded values. Evaluates `now` in the schedule's timezone against the schedule's peak windows.
2. **`WATTTIME_REGION = "PSCO"`** → Read from the schedule's `watttime_region` field.
3. **`MT = ZoneInfo("America/Denver")`** → Read timezone from the schedule.
4. **Single-device `get_device_id()`** → Iterate over all active devices in the registry, evaluate each against its own schedule, send per-device downlinks.

This is a straightforward refactor — the decision logic (pause if TOU peak OR MOER high) doesn't change, only where the parameters come from.

#### 4.5.6 Requirements

| Requirement | Status |
|-------------|--------|
| Meter number collected during commissioning | NOT STARTED |
| Meter number stored in device registry (`meter_number` field) | NOT STARTED (depends on TASK-036) |
| TOU schedule data model defined | DESIGNED (section 4.5.3) |
| TOU schedule table in DynamoDB | NOT STARTED |
| Charge scheduler reads per-device schedule from registry | NOT STARTED |
| Fallback to Xcel Colorado when no schedule configured | NOT STARTED (current behavior is the fallback) |
| Address → utility lookup (OpenEI API) | NOT STARTED (v1.1+) |
| Meter number → rate plan disambiguation | NOT STARTED (v1.1+) |

**Open question** (resolved): The meter number alone doesn't identify the utility — the install address does. The meter number identifies the specific rate plan within a utility. Both are captured during commissioning, but they serve different purposes in the lookup pipeline.

### 4.6 Infrastructure as Code

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

### 6.4 Warranty and Liability

#### 6.4.1 The Risk

SideCharge's core function requires physically intercepting two control circuits that belong to other manufacturers' equipment:

1. **J1772 pilot wire (EVSE ↔ vehicle)**: SideCharge taps the pilot signal between the EVSE and the vehicle and actively manipulates it — presenting resistance to the EVSE control circuit to force it into State B (pause charging), and potentially driving PWM to 0% for cloud override. This is not passive monitoring. It is active modification of the signaling protocol between the charger and the car.

2. **Thermostat call wire (thermostat ↔ compressor contactor)**: SideCharge intercepts the 24VAC call signal between the thermostat and the compressor contactor. When the interlock needs to block AC, it breaks this circuit.

Both are modifications to control circuits that belong to other manufacturers' equipment. The EVSE manufacturer, the vehicle manufacturer, and the HVAC manufacturer could each argue that a third-party device inserted into their control wiring constitutes unauthorized modification that voids the warranty.

#### 6.4.2 Risk Assessment by Circuit

**EVSE warranty — HIGH risk.** Most residential EVSE manufacturers (ChargePoint, JuiceBox, Wallbox, Emporia, Tesla Wall Connector, etc.) include warranty terms that exclude damage caused by "improper installation" or "unauthorized modification." Inserting a third-party device into the pilot wire is not an installation method contemplated by any EVSE manufacturer's installation manual. If the EVSE develops a fault — even one unrelated to SideCharge — the manufacturer could point to the pilot wire modification and deny the claim.

There is also a mechanical wear concern: SideCharge cycles the EVSE's internal relay (which disconnects AC power when it sees State B) more frequently than normal use. A car that normally charges uninterrupted overnight might now see 5-10 State B → State C transitions per day as the interlock and demand response toggle charging. Accelerated relay wear is a plausible SideCharge-caused defect, not just a warranty technicality.

**Vehicle warranty — MEDIUM risk.** SideCharge's pilot manipulation sends valid J1772 signals — the vehicle's onboard charger sees standard State B (pause) and State C (charge) transitions that it was designed to handle. The car doesn't know a third-party device is involved. However, if a charging-related defect occurs (onboard charger failure, battery management issue), a vehicle manufacturer could investigate the charging history and discover non-standard pilot signaling patterns. The risk is lower because the signals are protocol-compliant, but it is not zero.

**HVAC warranty — LOW risk.** Thermostat replacement and wiring modification is standard homeowner/contractor practice. HVAC manufacturers generally don't void warranties for thermostat-side changes. The only SideCharge-specific risk is if the interlock logic causes compressor cycling patterns outside the thermostat's built-in protection timer — but SideCharge passes the thermostat's call signal through (or blocks it entirely), so the thermostat's short-cycle protection remains in the loop.

#### 6.4.3 Legal Protection: Magnuson-Moss Warranty Act

The federal Magnuson-Moss Warranty Act (15 U.S.C. §§ 2301-2312) is SideCharge's primary legal defense:

- A manufacturer **cannot void a warranty** simply because a third-party product was installed
- The manufacturer **must prove** that the third-party product **caused** the specific defect being claimed
- "Tie-in sales provisions" (requiring OEM-only accessories) are generally prohibited for consumer products

This is the same law that protects aftermarket car parts, third-party phone accessories, and non-OEM components across industries. SideCharge has a strong legal position under MMWA for defects unrelated to the pilot wire or thermostat modifications.

**Where MMWA doesn't help**: If SideCharge's modifications actually cause the defect — accelerated relay wear from frequent State B cycling, signal integrity issues from the pilot wire tap, or a wiring error during installation — the manufacturer has a legitimate basis for denial. MMWA protects against blanket voiding, not against genuine causation.

#### 6.4.4 Mitigation Strategies

| # | Strategy | Effort | Risk Reduction | Timeline |
|---|----------|--------|----------------|----------|
| 1 | **J1772 protocol compliance** — All pilot manipulations use valid J1772 signaling (standard State A-F voltage levels, standard PWM). The EVSE and vehicle never see out-of-spec signals. | Done | Medium | v1.0 (current) |
| 2 | **Professional installation only** — Licensed electrician installs with commissioning checklist. Documented wiring, verified connections, installer sign-off. Reduces risk of installation-caused defects. | Low | Medium | v1.0 |
| 3 | **Customer disclosure** — Installation documentation explicitly states SideCharge modifies the EVSE pilot circuit and thermostat call wire, explains MMWA protections, recommends checking EVSE/vehicle warranty terms before installation. | Low | Low (legal CYA) | v1.0 |
| 4 | **Product liability insurance** — General commercial liability + product liability coverage for claims arising from SideCharge installations. | Medium ($2-5K/yr) | Medium (financial protection) | Pre-customer deployment |
| 5 | **Reversible connector design** — Redesign the pilot wire tap as a pass-through adapter with standard connectors. No wire cutting or splicing — the EVSE's original wiring is unmodified. Removal restores the original circuit instantly. | Medium | High | v1.1 |
| 6 | **Relay cycle logging** — Track how many State B ↔ State C transitions SideCharge causes per day/week. If an EVSE relay fails, we have data showing whether SideCharge's cycling was within the relay's rated lifetime. | Low | Medium (evidentiary) | v1.1 |
| 7 | **EVSE manufacturer partnerships** — Approach 1-2 EVSE manufacturers (Emporia and OpenEVSE are most accessible) for explicit compatibility acknowledgment or co-testing. | High | Very High | v1.1+ |
| 8 | **OCPP software integration** — For OCPP-capable EVSEs, control charging via the EVSE's own management API instead of hardware pilot manipulation. Eliminates the pilot wire modification entirely for compatible chargers. | High | Very High | v2.0 |

#### 6.4.5 Recommended Phased Approach

**v1.0 (current — early adopters, installer-owned properties)**:
Accept the warranty risk. Mitigate with strategies 1-3: protocol compliance (done), professional installation (planned), customer disclosure (add to commissioning docs). The first installations are on properties where the installer understands and accepts the tradeoff.

**Pre-customer deployment (before any non-installer customer)**:
Add strategy 4: product liability insurance. Non-negotiable before any customer installation. Budget $2-5K/year for a small commercial general liability + product liability policy. An insurance broker with IoT/hardware product experience can quote this.

**v1.1**:
Add strategies 5-6: reversible connector and relay cycle logging. The reversible connector is the single highest-impact mitigation — it changes the installation from "modified your EVSE wiring" to "plugged in an adapter." This fundamentally shifts the warranty argument. Relay cycle logging provides evidentiary defense if an EVSE relay fails.

**v1.1+ (growth phase)**:
Add strategy 7: EVSE manufacturer partnerships. Start with Emporia (small, US-based, receptive to integrations) or OpenEVSE (open-source hardware, community-driven). A single manufacturer's explicit compatibility acknowledgment validates the product category.

**v2.0 (scale)**:
Add strategy 8: OCPP integration. This is the long-term answer for compatible EVSEs. OCPP adoption is growing rapidly — ChargePoint, JuiceBox, Wallbox, and OpenEVSE all support it. Software-only control eliminates the pilot wire modification entirely. Hardware pilot control remains as the universal fallback for non-OCPP chargers.

#### 6.4.6 Open Questions

1. **EVSE relay wear**: Does SideCharge's cycling pattern (5-10 transitions/day from demand response + interlock) exceed the EVSE's internal relay rated lifetime? Most contactors are rated for 100K+ operations, which would be 27+ years at 10/day. But cheap relays in consumer EVSEs may have lower ratings. **Need to check spec sheets for target EVSE models.**

2. **Product positioning**: Should SideCharge be marketed as an "EVSE accessory" or an "energy management system"? The framing affects how EVSE manufacturers perceive the relationship. An "accessory" implies subordinate to the EVSE; an "energy management system" implies independent and authorized by code (NEC Article 750).

3. **Insurance scope**: Does product liability insurance cover warranty claims that a customer brings against us (i.e., "SideCharge told me it was safe and my EVSE warranty was voided"), or only claims for physical damage? Need to confirm scope with an insurance broker.

4. **State lemon laws and implied warranty**: Some states extend implied warranty protections beyond the manufacturer's written warranty. If SideCharge causes a defect within the implied warranty period, state law may provide the customer additional remedies. This varies by state and needs legal review.

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
- Multi-tariff or multi-utility TOU schedules (Xcel Colorado only for v1.0 — data model designed in 4.5, implementation is v1.1)

### 7.3 Future Considerations
- Fleet provisioning and management tooling
- Dashboard for real-time monitoring (DynamoDB → API Gateway → frontend)
- OTA image signing with ED25519 (keys already in MFG store)
- Battery-powered variant with sleep modes and reduced heartbeat
- Additional utility TOU schedules beyond Xcel Colorado (data model and top-5 schedules designed in 4.5)
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
| Charge scheduler hardcoded to Xcel Colorado | Cannot support second utility without code change | TASK-037 (designed in 4.5) |
| **EVSE/vehicle warranty risk from pilot wire modification** | SideCharge intercepts the J1772 pilot wire and actively manipulates signals. EVSE and vehicle manufacturers could deny warranty claims. Relay wear from frequent cycling is a plausible SideCharge-caused defect. See section 6.4. | TASK-043 (scoped in 6.4) |
| No product liability insurance | Controls high-power loads in residential settings. Property damage or injury creates liability exposure. Required before any customer deployment. | TASK-043 |
| No reversible connector design | Current prototype uses soldered/spliced pilot wire tap. Reversible pass-through adapter eliminates the "modified wiring" argument. Highest-impact warranty mitigation. | TASK-043 (v1.1) |

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
| TASK-037 | 4.5 (Utility Identification) | Per-device meter number → utility → TOU schedule lookup |
| TASK-043 | 6.4 (Warranty and Liability) | Warranty risk scoping, mitigation roadmap, insurance, reversible connector, OCPP path |
