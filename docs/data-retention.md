# SideCharge Data Retention and Deletion Procedures

**Internal Document — Not Customer-Facing**
**Last Updated**: 2026-02-19
**Owner**: Pam (Product Manager) — pending external privacy consultant assignment
**PRD Reference**: Section 6.4.2

---

## 1. Data Retention Rules

### 1.1 Telemetry Events (`sidewalk-v1-device_events_v2`)

| Rule | Value | Mechanism |
|------|-------|-----------|
| Raw retention | **90 days** | DynamoDB TTL on `ttl` attribute |
| TTL attribute | `ttl` = `floor(timestamp_ms / 1000) + 7776000` | Set by decode Lambda on every `put_item` |
| DynamoDB deletion | Automatic, within ~48 hours of TTL expiry | AWS-managed background process |
| Aggregation window | Days 85–90 (before TTL expiry) | Aggregation Lambda scans and summarizes |

**How TTL works**: The decode Lambda sets a `ttl` attribute (Unix epoch seconds) on every DynamoDB item equal to the event timestamp plus 90 days (7,776,000 seconds). DynamoDB's built-in TTL process automatically deletes expired items. No manual cleanup required.

### 1.2 Daily Aggregates (`sidecharge-daily-aggregates` — TASK-078)

| Rule | Value | Mechanism |
|------|-------|-----------|
| Retention | **3 years** | DynamoDB TTL on `ttl` attribute |
| TTL attribute | `ttl` = `date_epoch + 94608000` (3 years in seconds) | Set by aggregation Lambda |
| Schedule | Daily at **02:00 UTC** | EventBridge cron rule |
| Zero-event days | No record written | Graceful skip |

**Aggregate fields (per device per day):**

| Category | Field | Type | Notes |
|----------|-------|------|-------|
| Energy | `total_kwh` | Decimal | Assumes 240V mains (`ASSUMED_VOLTAGE_V` env var) |
| Energy | `peak_current_a` | Decimal | Highest instantaneous current reading |
| EV Charging | `charge_sessions` | int | Transitions into J1772 State C |
| EV Charging | `charge_minutes` | Decimal | Total minutes in State C |
| AC Compressor | `ac_compressor_minutes` | Decimal | Total minutes with `ac_compressor=true` |
| Faults | `fault_count_sensor` | int | Runtime `FAULT_SENSOR` (0x10) occurrences |
| Faults | `fault_count_clamp` | int | Runtime `FAULT_CLAMP` (0x20) occurrences |
| Faults | `fault_count_interlock` | int | Runtime `FAULT_INTERLOCK` (0x40) occurrences |
| Faults | `selftest_passed` | bool | All 4 hardware checks passed on boot |
| Availability | `event_count` | int | Total uplinks received |
| Availability | `availability_pct` | Decimal | `event_count / 96 * 100` (96 = expected 15-min heartbeats/day) |
| Availability | `longest_gap_minutes` | Decimal | Longest gap between consecutive uplinks (or midnight boundary) |

**Implementation**: `aws/aggregation_lambda.py` (TASK-078, deployed 2026-02-19).

### 1.3 Device Registry (`sidecharge-device-registry` — TASK-036)

| Rule | Value | Mechanism |
|------|-------|-----------|
| Active device PII | Retained for active service life | No TTL — deleted on return/decommission |
| Post-return grace period | **30 days** | Deletion Lambda triggered by status change to `returned` |
| PII fields deleted | `owner_name`, `owner_email`, `install_address`, `install_lat`, `install_lon`, `meter_number`, `installer_name` | Deletion Lambda removes attributes |
| Hardware record retained | `device_id`, `sidewalk_id`, `provisioned_date`, `status`, `deleted_at` | Permanent — asset tracking |

### 1.4 CloudWatch Logs

| Log Group | Retention | Terraform Resource |
|-----------|-----------|-------------------|
| `/aws/lambda/uplink-decoder` | **30 days** | `aws_cloudwatch_log_group.uplink_decoder_logs` |
| `/aws/lambda/charge-scheduler` | **30 days** | `aws_cloudwatch_log_group.charge_scheduler_logs` |
| `/aws/lambda/ota-sender` | **30 days** | `aws_cloudwatch_log_group.ota_sender_logs` |
| `/aws/lambda/health-digest` | **30 days** | `aws_cloudwatch_log_group.health_digest_logs` |
| `/aws/lambda/daily-aggregation` | **30 days** | `aws_cloudwatch_log_group.aggregation_logs` |

### 1.5 OTA Session Data

| Rule | Value | Mechanism |
|------|-------|-----------|
| OTA session records in DynamoDB | **30 days** | DynamoDB TTL (set by OTA sender Lambda) |
| Firmware binaries in S3 | Indefinite | Manual cleanup of deprecated versions |

### 1.6 TIME_SYNC Sentinel Records

| Rule | Value | Mechanism |
|------|-------|-----------|
| Sentinel records (`timestamp=-2`) | **No TTL** | Overwritten on each sync; one per device |

These are operational records (last sync timestamp per device), not telemetry. They contain no PII. One record per device, overwritten each sync cycle.

---

## 2. Customer Data Deletion Procedure

### 2.1 Normal Device Return / Service Cancellation

| Step | Action | Owner | Timeline |
|------|--------|-------|----------|
| 1 | Customer contacts support or installer initiates return | Customer / Installer | — |
| 2 | Operator sets device registry status to `returned`, records `return_date` | Operator | 1 business day |
| 3 | Grace period — device record retained in case of re-installation | Automated | 30 days |
| 4 | Deletion Lambda removes PII from registry: `owner_name`, `owner_email`, `install_address`, `install_lat`, `install_lon`, `meter_number`, `installer_name` | Automated | Within 48 hours of grace period expiry |
| 5 | Deletion Lambda deletes all telemetry events for that `device_id` from events table | Automated | Same batch as step 4 |
| 6 | Deletion Lambda deletes all daily aggregates for that `device_id` | Automated | Same batch as step 4 |
| 7 | Hardware record retained: `device_id`, `sidewalk_id`, `provisioned_date`, `status=returned`, `deleted_at` | Permanent | — |
| 8 | Operator confirms deletion to customer via email | Operator | 5 business days after deletion |

### 2.2 CCPA Right to Delete Request

Same as section 2.1 except:
- **No 30-day grace period** — deletion begins immediately
- **45-day SLA** — deletion must complete within 45 calendar days of request (CCPA requirement)
- **One 45-day extension** permitted if necessary, but must notify consumer with reason
- **Confirmation required** — written confirmation of deletion to the consumer

### 2.3 What Is NOT Deleted

The device hardware record is retained permanently for asset tracking and warranty:
- `device_id` (SC-XXXXXXXX)
- `sidewalk_id` (full AWS UUID)
- `provisioned_date`
- `status` (set to `returned`)
- `deleted_at` (timestamp of PII deletion)

This is permitted under CCPA's exception for data necessary to complete the transaction or maintain the product (Cal. Civ. Code 1798.105(d)(1)).

### 2.4 Deletion Lambda (Not Yet Implemented)

The deletion Lambda will:
1. Be triggered by EventBridge on a daily schedule
2. Query registry for devices with `status=returned` and `return_date + 30 days < now`
3. For each qualifying device:
   - Remove PII attributes from registry record (UpdateItem, REMOVE)
   - Query and batch-delete all events for that `device_id` from events table
   - Query and batch-delete all aggregates for that `device_id` from aggregates table
   - Set `deleted_at` timestamp on registry record
4. Log deletion actions (device_id only, no PII) for audit trail

---

## 3. CCPA Compliance Checklist

### 3.1 Threshold Analysis

| Criterion | SideCharge Status | CCPA Applies? |
|-----------|-------------------|---------------|
| Annual revenue > $25M | Pre-revenue | No (currently) |
| Buy/sell personal data of 100K+ consumers | Single-device deployment | No (currently) |
| 50%+ revenue from selling personal data | No data sales | No |

**Current status**: CCPA does not apply to SideCharge today. However, we design for compliance because (a) the parent business entity may meet thresholds from other products, and (b) Colorado CPA has no revenue threshold for data controllers.

### 3.2 Consumer Rights Compliance

| Right | Status | Gap | Remediation |
|-------|--------|-----|-------------|
| Right to Know | PARTIAL | Privacy policy drafted but not published; no delivery mechanism for individual data reports | Publish privacy policy; build data export procedure |
| Right to Delete | DESIGNED | Deletion procedure defined (section 2) but deletion Lambda not yet implemented | Implement deletion Lambda (TASK-038 follow-up) |
| Right to Correct | PARTIAL | Operator can update registry manually; no formal procedure | Document procedure, assign operator responsibility |
| Right to Opt-Out of Sale | COMPLIANT | SideCharge does not sell data | No action needed; revisit if business model changes |
| Right to Non-Discrimination | COMPLIANT | No pricing tiers or service levels tied to data consent | No action needed |
| Right to Limit Sensitive Data | GAP | Install address and lat/lon are "precise geolocation" under CCPA; no opt-out mechanism | Review with privacy consultant — may need to limit geolocation to utility region only |
| Breach Notification (72h) | GAP | No incident response plan | Draft incident response plan; engage consultant |

### 3.3 Multi-State Coverage

| Law | Effective | Key Differences from CCPA | SideCharge Impact |
|-----|-----------|---------------------------|-------------------|
| California CCPA/CPRA | Jan 2023 | Baseline — broadest "sale" definition | Primary compliance target |
| Colorado CPA | Jul 2023 | No revenue threshold for data controllers; opt-out of targeted advertising | May apply sooner than CCPA if SideCharge has Colorado customers (which it does — Xcel/PSCO) |
| Virginia VCDPA | Jan 2023 | 100K consumer threshold; narrower "sale" definition | Unlikely to apply at current scale |
| Connecticut CTDPA | Jul 2023 | Similar to CPA; includes data minimization requirement | Data minimization aligns with our privacy-by-design principles |

**Key risk**: Colorado CPA may apply to SideCharge before CCPA does, since the first deployment is in Colorado (Xcel/PSCO region) and CPA has no revenue threshold for data controllers who process personal data. Confirm with privacy consultant.

### 3.4 Specific Obligations Checklist

| Obligation | Status | Notes |
|------------|--------|-------|
| Privacy policy published and accessible | NOT DONE | Draft at `docs/privacy-policy.md`; needs legal review and hosting |
| Privacy policy updated annually | NOT DONE | Set calendar reminder after publication |
| "Do Not Sell" link (if applicable) | N/A | SideCharge does not sell data |
| Data processing agreements with service providers | PARTIAL | AWS standard terms cover DPA; WattTime receives no PII (no DPA needed) |
| Data processing inventory | DONE | PRD 6.4.2.1 documents all data categories, sources, and third-party flows |
| Breach notification plan | NOT DONE | Need incident response plan with 72-hour notification procedure |
| Consumer request intake process | NOT DONE | Need email/form for privacy requests; assign response owner |
| Response SLA tracking | NOT DONE | Need system to track 45-day response deadline |

---

## 4. CloudWatch PII Audit Checklist

Must be completed before first customer deployment.

| Lambda | Check | Status | Notes |
|--------|-------|--------|-------|
| `decode_evse_lambda.py` | Logs Sidewalk device UUID? | OK | Device UUID is pseudonymous, not PII |
| `decode_evse_lambda.py` | Logs decoded payload data? | OK | Payload contains J1772/current/thermostat — no PII |
| `decode_evse_lambda.py` | Logs raw Sidewalk event? | OK | Line 407: `print(f"Received event: {json.dumps(event)}")` — Sidewalk event contains WirelessDeviceId, PayloadData, WirelessMetadata (link type, RSSI, seq). No PII. **Re-verify if registry fields are ever added to the event path.** |
| `decode_evse_lambda.py` | Logs registry fields? | OK | Registry integration (TASK-036) uses `device_registry.py` which only logs SC-XXXXXXXX short ID. No Tier 1 fields logged. |
| `charge_scheduler_lambda.py` | Logs device ID? | OK | Device UUID is pseudonymous |
| `charge_scheduler_lambda.py` | Logs meter number or address? | N/A | Utility lookup not yet integrated (TASK-037). **When integrated: must NOT log meter_number or install_address** |
| `ota_sender_lambda.py` | Any PII exposure? | OK | Operates on device ID and binary data only |
| `health_digest_lambda.py` | Logs device details? | OK | Logs only SC-XXXXXXXX device IDs, last-seen, app versions, and fault types. No PII fields read or logged. |
| `aggregation_lambda.py` | Any PII exposure? | OK | Reads only telemetry events and device registry `device_id`/`wireless_device_id`. No Tier 1 fields accessed. Logs only device counts and aggregate stats. |

**Rule**: Lambda code must never log Tier 1 fields (owner_name, owner_email, install_address, install_lat, install_lon, meter_number, installer_name). Log only the pseudonymous device_id (SC-XXXXXXXX or Sidewalk UUID). Enforce in code review.

---

## 5. Implementation Status

| Deliverable | Status | Blocking Task |
|-------------|--------|---------------|
| DynamoDB TTL on events table (Terraform) | IMPLEMENTED | — (already existed) |
| Decode Lambda sets `ttl` attribute on items | IMPLEMENTED | TASK-038 |
| CloudWatch log retention updated to 30 days | IMPLEMENTED | TASK-038 |
| Privacy policy draft | DONE | TASK-038 |
| Data retention procedures | DONE | TASK-038 (this document) |
| CCPA compliance checklist | DONE | TASK-038 (this document) |
| CloudWatch PII audit | DONE | TASK-038 (section 4 of this document) |
| Aggregation Lambda (daily summaries) | DEPLOYED | TASK-078 (2026-02-19) |
| Deletion Lambda (PII + telemetry cleanup) | NOT STARTED | Future (pre-multi-customer) |
| Device registry table | DEPLOYED | TASK-036 (merged) + TASK-049 (terraform applied) |
| External privacy consultant engagement | NOT STARTED | TASK-042 recommendation |
| Incident response / breach notification plan | NOT STARTED | Consultant deliverable |
