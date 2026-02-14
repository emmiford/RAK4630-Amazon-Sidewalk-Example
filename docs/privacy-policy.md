# SideCharge Privacy Policy

**Effective Date**: [TBD — before first customer deployment]
**Last Updated**: 2026-02-13
**Status**: DRAFT — Requires external privacy consultant review before publication

---

## 1. Who We Are

SideCharge is a home energy management device that monitors your EV charger and air conditioning system to help you save money by shifting electricity usage to off-peak hours. This privacy policy explains what data we collect, why we collect it, how long we keep it, and your rights regarding that data.

## 2. What Data We Collect

### 2.1 Information You Provide During Installation

When your electrician installs a SideCharge device, they record:

- **Your name and email address** — so we can contact you about service issues or device updates
- **Installation address** — so we know which utility serves your home and which rate schedule applies
- **Electric meter number** — so we can look up your utility's time-of-use rates to optimize your charging schedule
- **Installer name and date** — for the installation record

### 2.2 Information the Device Collects Automatically

The SideCharge device monitors your electrical panel and sends the following data to our cloud service approximately every 5 minutes:

- **EV charger status** — whether your car is plugged in, charging, or disconnected (J1772 pilot state)
- **Current draw** — how many amps your EV charger is using
- **Air conditioning status** — whether your AC compressor is running (on/off only, not temperature settings)
- **Device health data** — firmware version, connection status, self-test results

### 2.3 Information We Do NOT Collect

- We do not collect your electricity bill amount or utility account credentials
- We do not collect your driving habits, destinations, or vehicle information beyond charging status
- We do not collect indoor temperature, thermostat setpoints, or any comfort settings
- The SideCharge device has no camera, microphone, GPS, or WiFi radio
- **The device itself stores none of your personal information.** If the device is lost, stolen, or discarded, it reveals nothing about you.

## 3. How We Use Your Data

| Data | Purpose |
|------|---------|
| Name and email | Contact you about service updates, device alerts, or support |
| Installation address | Determine your utility region for rate schedule lookup |
| Meter number | Look up your specific utility time-of-use rate schedule |
| EV charger status and current draw | Determine when to allow or pause charging based on electricity rates and grid conditions |
| AC compressor status | Coordinate EV charging around AC usage to avoid overloading your electrical panel |
| Device health data | Monitor device operation, deliver firmware updates, diagnose problems |

We do not use your data for advertising, profiling, or any purpose other than operating the SideCharge service.

## 4. How Long We Keep Your Data

| Data | Retention Period |
|------|-----------------|
| Detailed device readings (per-minute EV/AC status, current draw) | **90 days**, then automatically deleted |
| Daily summaries (total kWh charged, AC hours, session counts) | **3 years** |
| Your personal information (name, email, address, meter number) | Until you cancel service or return the device, plus a 30-day grace period |
| Device operational logs | **30 days** |

After the retention period, data is automatically and permanently deleted. We do not move expired data to archive or backup storage.

## 5. Who We Share Your Data With

**We do not sell your personal information.** We do not share your data with advertisers or data brokers.

We use the following service providers to operate SideCharge:

| Provider | What They Process | Why |
|----------|-------------------|-----|
| Amazon Web Services (AWS) | All cloud-side data (encrypted at rest and in transit) | Cloud infrastructure — data storage, processing, device communication |
| WattTime | Grid region identifier only (e.g., "PSCO") — **no personal information** | Real-time grid carbon intensity signal for clean energy optimization |

Amazon and WattTime process data on our behalf under their standard service agreements. They do not use your data for their own purposes.

## 6. How We Protect Your Data

- **Encryption in transit**: All communication between your SideCharge device and our cloud service is encrypted using Amazon Sidewalk's built-in encryption
- **Encryption at rest**: All stored data is encrypted using AWS default encryption (AES-256)
- **Access control**: Only authorized operators can access customer data, through AWS Identity and Access Management (IAM)
- **No personal data on device**: The SideCharge device stores no personal information — your name, address, and meter number exist only in our secure cloud database
- **Automatic data expiration**: Detailed telemetry data is automatically deleted after 90 days

## 7. Your Privacy Rights

Depending on where you live, you may have the following rights:

### 7.1 Right to Know
You can request a summary of what personal information we have collected about you and how we use it. Contact us and we will provide this within 45 days.

### 7.2 Right to Delete
You can request that we delete your personal information. We will delete your name, email, address, meter number, and all device telemetry data associated with your account. We will retain only the device hardware record (device ID and manufacturing date) for asset tracking and warranty purposes. Deletion will be completed within 45 days of your request.

### 7.3 Right to Correct
If any of your personal information is inaccurate, contact us and we will correct it.

### 7.4 Right to Opt-Out of Sale
We do not sell your personal information, so there is nothing to opt out of. If this ever changes, we will update this policy and provide an opt-out mechanism before any change takes effect.

### 7.5 Non-Discrimination
We will not penalize you for exercising any of your privacy rights. All SideCharge devices receive the same service regardless of privacy choices.

## 8. Device Return and Decommission

When you return a SideCharge device or cancel service:

1. Your personal information (name, email, address, meter number) is deleted after a 30-day grace period
2. All telemetry data associated with your device is permanently deleted
3. The returned device is factory-reset and contains no record of your information
4. You will receive confirmation when deletion is complete

## 9. Children's Privacy

SideCharge is a home electrical infrastructure device installed by licensed electricians. It is not directed at children and we do not knowingly collect personal information from anyone under 16.

## 10. Changes to This Policy

We will notify you by email before making material changes to this privacy policy. The updated policy will be posted at [URL TBD] with a new effective date.

## 11. Contact Us

For privacy questions, data requests, or to exercise your rights:

- **Email**: [privacy@sidecharge.com — TBD]
- **Mail**: [Mailing address — TBD]

We will respond to all privacy requests within 45 calendar days.

---

*This privacy policy is designed to comply with the California Consumer Privacy Act (CCPA), Colorado Privacy Act (CPA), Virginia Consumer Data Protection Act (VCDPA), and Connecticut Data Privacy Act (CTDPA). It has not yet been reviewed by legal counsel. External privacy consultant review is required before publication.*
