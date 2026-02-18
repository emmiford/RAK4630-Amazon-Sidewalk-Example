# SideCharge Privacy Governance

**Internal Document — Not Customer-Facing**
**Last Updated**: 2026-02-17
**Owner**: Pam (Product Manager)
**PRD Reference**: Section 6.4.2
**Prerequisite**: TASK-038 (data retention, privacy policy, CCPA checklist)

---

## 1. Privacy Agent Assignment

### 1.1 Current State

SideCharge is a single-device, pre-revenue product. A full-time privacy officer or external legal counsel is not yet justified. Instead, privacy governance follows a **distributed ownership model** with defined escalation paths.

### 1.2 Privacy Roles

| Role | Owner | Responsibilities |
|------|-------|------------------|
| **Privacy Lead** | Pam (Product Manager) | Owns privacy policy, data retention rules, CCPA compliance checklist, privacy impact assessments for new features |
| **Technical Privacy Reviewer** | Eliel (Backend Architect) | Reviews code changes touching PII fields, enforces "no PII in logs" rule, implements deletion Lambda, manages Terraform TTL settings |
| **Incident Response Lead** | Eliel (Backend Architect) | First responder for data breach or unauthorized access; escalates to privacy consultant if needed |
| **External Privacy Consultant** | TBD — engage before first multi-customer deployment | Legal review of privacy policy, CCPA/CPA gap analysis, incident response plan |

### 1.3 When to Engage External Counsel

Engage an external privacy consultant (attorney or certified privacy professional) before:

- First deployment to a customer who is not the product developer
- Crossing any CCPA/CPA applicability threshold (revenue, consumer count)
- Any data breach or unauthorized access incident
- Adding a new third-party data processor beyond AWS and WattTime
- Collecting any new category of personal information
- Entering a new state/jurisdiction with distinct privacy laws

**Budget estimate**: $2,000–$5,000 for initial review + annual $1,000–$2,000 retainer. Consider organizations like the International Association of Privacy Professionals (IAPP) for consultant referrals.

---

## 2. Data Inventory

### 2.1 Personal Information Collected

| Category | Fields | Storage | Retention | Sensitivity |
|----------|--------|---------|-----------|-------------|
| **Contact info** | owner_name, owner_email | Device registry (DynamoDB) | Active service + 30-day grace | Tier 1 (PII) |
| **Location** | install_address, install_lat, install_lon | Device registry (DynamoDB) | Active service + 30-day grace | Tier 1 (PII — "precise geolocation" under CCPA) |
| **Utility info** | meter_number | Device registry (DynamoDB) | Active service + 30-day grace | Tier 1 (PII) |
| **Installer info** | installer_name, provisioned_date | Device registry (DynamoDB) | Active service + 30-day grace | Tier 1 (PII) |

### 2.2 Behavioral Data Collected

| Category | Fields | Storage | Retention | Sensitivity |
|----------|--------|---------|-----------|-------------|
| **EV charging** | J1772 pilot state, current draw (mA) | Events table (DynamoDB) | 90 days (TTL) | Medium — reveals occupancy patterns |
| **AC usage** | Compressor on/off | Events table (DynamoDB) | 90 days (TTL) | Medium — reveals occupancy patterns |
| **Device health** | Firmware version, boot count, faults | Events table (DynamoDB) | 90 days (TTL) | Low |

### 2.3 Third-Party Data Flows

| Recipient | Data Shared | Purpose | DPA Required? |
|-----------|-------------|---------|---------------|
| AWS | All cloud data (encrypted) | Infrastructure | Covered by AWS standard terms |
| WattTime | Grid region ID only (e.g., "PSCO") | Carbon intensity signal | No — no PII shared |

---

## 3. Privacy Review Process

### 3.1 When Privacy Review Is Required

A privacy review is required before merging any PR that:

1. **Adds or modifies PII fields** in the device registry or any data store
2. **Adds a new DynamoDB table** or S3 bucket that stores customer data
3. **Adds a new third-party integration** that receives any customer data
4. **Changes data retention** periods (TTL values, log retention)
5. **Adds new logging** in Lambda functions that could expose PII
6. **Modifies the deletion** or decommission procedure

### 3.2 Privacy Review Checklist (for PR reviewers)

- [ ] No Tier 1 fields (owner_name, owner_email, install_address, install_lat, install_lon, meter_number, installer_name) appear in any `print()` or logging statement
- [ ] New DynamoDB items include a `ttl` attribute matching the retention policy
- [ ] New CloudWatch log groups have `retention_in_days = 30` in Terraform
- [ ] New third-party APIs do not receive PII (or a DPA is in place)
- [ ] Privacy policy accurately describes any new data collection
- [ ] data-retention.md is updated if retention rules change

### 3.3 Review Cadence

| Review | Frequency | Owner | Trigger |
|--------|-----------|-------|---------|
| Privacy policy accuracy check | Quarterly | Pam | Calendar (Jan, Apr, Jul, Oct) |
| CloudWatch PII audit | Quarterly | Eliel | Calendar + any Lambda code change |
| CCPA/CPA compliance re-assessment | Annually | Pam + consultant | Calendar (January) |
| Data retention verification | Semi-annually | Eliel | Calendar (Jan, Jul) — verify TTLs working, no orphaned data |
| Third-party processor inventory | Annually | Pam | Calendar (January) |

---

## 4. Consumer Request Handling

### 4.1 Request Types

| Request | CCPA Right | Response SLA | Process |
|---------|------------|-------------|---------|
| "What data do you have on me?" | Right to Know | 45 calendar days | Query device registry by owner_email; export telemetry for associated device_id |
| "Delete my data" | Right to Delete | 45 calendar days | Set device status=returned; PII deleted after 30-day grace (or immediately for CCPA requests) |
| "Correct my information" | Right to Correct | 45 calendar days | Operator updates registry fields manually |
| "Stop selling my data" | Right to Opt-Out of Sale | Immediate | N/A — SideCharge does not sell data |

### 4.2 Request Intake

**Current process** (pre-multi-customer):
- Privacy requests sent to operator email (TBD: privacy@sidecharge.com)
- Operator logs request in a spreadsheet with: date received, request type, requester identity, deadline (45 days), completion date
- One 45-day extension permitted if necessary (CCPA) — must notify requester with reason

**Future process** (post-multi-customer):
- Dedicated privacy request web form
- Automated SLA tracking with deadline alerts
- Automated data export for "Right to Know" requests

### 4.3 Identity Verification

Before fulfilling any consumer request, verify the requester's identity:
1. Match requester email to `owner_email` in device registry
2. If email doesn't match, request additional verification (install address, device serial number)
3. Never share data with an unverified requester

---

## 5. Incident Response (Data Breach)

### 5.1 Current Status

**No formal incident response plan exists.** This is a known gap (see `docs/data-retention.md` §3.2). An incident response plan should be developed with external privacy counsel before multi-customer deployment.

### 5.2 Interim Procedure

Until a formal plan is in place:

| Step | Action | Timeline | Owner |
|------|--------|----------|-------|
| 1 | Detect: Monitor CloudWatch alarms, review access logs | Ongoing | Eliel |
| 2 | Contain: Revoke compromised credentials, isolate affected systems | Within 1 hour | Eliel |
| 3 | Assess: Determine what data was accessed, how many consumers affected | Within 24 hours | Pam + Eliel |
| 4 | Notify: If breach affects 500+ consumers, notify CA AG within 72 hours (CCPA). Notify affected consumers "without unreasonable delay." | Within 72 hours | Pam |
| 5 | Remediate: Fix vulnerability, rotate credentials, update security controls | Within 1 week | Eliel |
| 6 | Document: Record incident details, root cause, remediation, prevention | Within 2 weeks | Pam |

### 5.3 Notification Requirements by State

| State | Notification Deadline | Authority | Threshold |
|-------|----------------------|-----------|-----------|
| California (CCPA) | 72 hours | CA Attorney General | 500+ consumers |
| Colorado (CPA) | 30 days | CO Attorney General | 500+ consumers |
| Virginia (VCDPA) | "Without unreasonable delay" | VA Attorney General | Any number |
| Connecticut (CTDPA) | 60 days | CT Attorney General | Any number |

---

## 6. Open Items and Roadmap

| Item | Priority | Target | Owner |
|------|----------|--------|-------|
| Engage external privacy consultant | High | Before first non-developer customer | Pam |
| Implement deletion Lambda | High | Before first non-developer customer | Eliel |
| Implement daily aggregation Lambda | Medium | Before multi-customer rollout | Eliel |
| Build consumer request intake form | Medium | Before multi-customer rollout | Pam |
| Publish privacy policy (web hosting) | Medium | Before first non-developer customer | Pam |
| Develop formal incident response plan | Medium | Before multi-customer rollout | Pam + consultant |
| Add geolocation opt-out mechanism | Low | Before 10+ customers in CA | Eliel + consultant |
| Automate data export for Right to Know | Low | Before 50+ customers | Eliel |
