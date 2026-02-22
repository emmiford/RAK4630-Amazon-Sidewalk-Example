# TASK-037: Per-Device Utility Identification

**Status**: Scoping
**Author**: Pam (Product Manager)
**Date**: 2026-02-13
**PRD refs**: Section 6.4.1 (Utility Identification), Section 4.4 (Demand Response)

---

## Problem Statement

The charge scheduler Lambda (`charge_scheduler_lambda.py`) has Xcel Energy Colorado's TOU schedule hardcoded (`is_tou_peak`: weekdays 5-9 PM MT) and uses a hardcoded WattTime balancing authority (`WATTTIME_REGION = "PSCO"`). The `get_device_id()` helper in `sidewalk_utils.py` picks a single device by name heuristic ("eric"). This works for one device in Colorado Springs but cannot support a second device at a different utility, a different rate plan, or even a different balancing authority.

When SideCharge ships to multiple homes, the cloud must know each device's utility and rate schedule to apply the correct TOU windows and grid carbon signal.

---

## 1. How Does the System Learn a Device's Utility?

### Options Evaluated

#### (a) Installer enters ZIP code at commissioning; cloud maps ZIP to utility

- **Pros**: Simple for the installer. ZIP codes are well-known. The NREL/OpenEI ZIP-to-utility dataset (EIA-861 based) provides a free lookup.
- **Cons**: ~15% of US ZIP codes are served by more than one utility (overlapping service territories, municipal vs. IOU boundaries). ZIP 80903 (Colorado Springs) is served by both Colorado Springs Utilities (city) and Xcel/PSCO (county areas). The installer would need to disambiguate manually in those cases.
- **Data source**: NREL "U.S. Electric Utility Companies and Rates: Look-up by Zip Code (2024)" -- free CSV download from OEDI. No live API; it is a static dataset updated annually.

#### (b) Installer enters utility account number or meter number

- **Pros**: Unambiguous. The meter number uniquely identifies the utility, the service point, and (in many cases) the rate class. The PRD already calls for a `meter_number` field in the device registry (section 4.6). The electrician is standing at the meter during installation -- it is physically visible.
- **Cons**: Requires the installer to read and type a 10-20 digit meter number. There is no universal API to look up "meter number X belongs to utility Y" -- each utility has its own format. We would need a manual or semi-manual mapping step (or a curated lookup table per utility).
- **Note**: The meter number is useful for customer support and future Green Button / utility API integration, but it does not by itself give us the utility identity without a mapping layer.

#### (c) Installer selects utility from a dropdown list

- **Pros**: Unambiguous (if the list is correct). Simple UX -- the installer taps "Xcel Energy" or "Colorado Springs Utilities" from a list filtered by state or ZIP.
- **Cons**: Requires maintaining a utility database. There are ~3,300 electric utilities in the US (EIA-861). For v1.0 with <10 sites in one metro area, a full database is overkill. However, a short list of supported utilities (initially just Xcel Colorado) is trivial.
- **Hybrid**: Filter by ZIP to show the 1-3 utilities serving that area, then the installer picks.

#### (d) Device lat/lon from the device registry; cloud maps coordinates to utility via API

- **Pros**: Fully automatic -- no installer action beyond providing the install address (already required for the device registry, PRD section 4.6). The NREL Utility Rates API (`api.openei.org/utility_rates`) accepts lat/lon and returns matching utilities. WattTime's `/v3/region-from-loc` endpoint returns the balancing authority for a lat/lon. This solves both the utility lookup and the BA mapping in one step.
- **Cons**: Requires geocoded coordinates. If the installer enters a street address, we need a geocoding step (Google Maps Geocoding API, free tier: 28k requests/month; or US Census Geocoder, free, no key). Multi-utility ZIP codes still return multiple results -- we would need a disambiguation rule or installer confirmation.
- **Accuracy**: NREL API uses geospatial utility service territory boundaries (better than ZIP code approximation). For rural/edge cases, the match may still be ambiguous.

### Recommendation for v1.0

**Option (c) with a static short list, seeded by Option (a) for convenience.**

Rationale:
- The first 10 installations are all in Colorado Springs, all on Xcel Energy (PSCO). We do not need a comprehensive utility database yet.
- The device registry commissioning form (TASK-036) already collects `install_address`. We add a `utility_id` field (e.g., `"xcel_co"`) and a `rate_plan` field (e.g., `"re_tou"`).
- The installer selects from a short dropdown: `["Xcel Energy (Colorado)", "Colorado Springs Utilities"]`. This list is hardcoded in the commissioning tool and maps directly to a TOU schedule config.
- We also collect `meter_number` (as the PRD already specifies) for future use, but do not use it for utility identification in v1.0.
- When we expand beyond Colorado (>10 utilities), we graduate to Option (d) -- lat/lon lookup via NREL API with installer confirmation for ambiguous matches.

**What we are NOT doing for v1.0**: No automated meter-number-to-utility lookup. No OpenEI API integration. No Arcadia/Genability subscription. These are overkill for 10 devices at one utility.

---

## 2. TOU Schedule Database

### Options Evaluated

#### (a) Hardcoded per utility in Lambda code

- **Current state**: `is_tou_peak()` checks `weekday < 5 and 17 <= hour < 21`. This is Xcel Colorado's RE-TOU peak window.
- **Pros**: Zero infrastructure. Works today.
- **Cons**: Adding a second utility means adding another `if` branch. At 5+ utilities, the Lambda becomes a mess. Rate plans change (Xcel changed peak hours from 2-7 PM to 5-9 PM in 2024). No versioning, no audit trail.
- **Verdict**: Acceptable for <5 utilities. Unacceptable beyond that.

#### (b) DynamoDB table mapping utility_id to TOU schedule

- **Pros**: Scalable. The scheduler Lambda reads the device's `utility_id` from the device registry, then looks up the TOU schedule from a `tou_schedules` table. Adding a new utility is a DynamoDB put_item, not a code deploy. Schedule changes are data updates, not code changes.
- **Cons**: Someone must enter the TOU data. For v1.0 with 1-2 utilities, this is manual (we type in the peak hours from the utility's tariff book).
- **Schema** (proposed):
  ```
  Table: evse-tou-schedules
  PK: utility_id (String, e.g. "xcel_co")
  SK: rate_plan (String, e.g. "re_tou")

  Attributes:
    timezone: "America/Denver"
    peak_weekdays: [0,1,2,3,4]     # Mon-Fri (Python weekday())
    peak_start_hour: 17              # 5 PM local
    peak_end_hour: 21                # 9 PM local
    peak_months: [1,2,3,4,5,6,7,8,9,10,11,12]  # year-round
    effective_date: "2024-10-01"     # when this schedule took effect
    source_url: "https://www.xcelenergy.com/..."  # tariff source
  ```
- **Cost**: DynamoDB PAY_PER_REQUEST. At 10 devices x 1 scheduler invocation/5 min = 2,880 reads/day. Free tier covers 25 RCU perpetually. Cost: $0.

#### (c) OpenEI USURDB API (real-time lookup)

- **Pros**: Comprehensive (3,700+ utilities, thousands of rate plans). Includes `energyweekdayschedule` and `energyweekendschedule` arrays with per-hour period indices for every month. Free, no API key required (NREL key optional).
- **Cons**: Complex response format -- the rate structures are designed for bill calculation, not simple "is it peak right now?" queries. Parsing TOU periods out of the USURDB schema requires mapping period indices to rate tiers, then determining which tier is "peak." NREL updates the database ~annually; rates can change mid-year. The API is a lookup, not a real-time service -- we would need to cache results.
- **Verdict**: Excellent long-term data source for populating the DynamoDB table (Option b). Not suitable as a real-time query on every scheduler invocation.

### Recommendation for v1.0

**Option (b): DynamoDB table, manually populated for the first 1-2 utilities.**

- Add a `evse-tou-schedules` DynamoDB table via Terraform.
- Seed it with Xcel Colorado RE-TOU data (peak: weekdays 5-9 PM MT, year-round).
- The scheduler Lambda reads the device's `utility_id` + `rate_plan` from the device registry, then reads the TOU schedule from this table.
- When we add the 11th utility, consider a batch import script that reads USURDB data and writes to DynamoDB. That is a future task, not v1.0.

---

## 3. WattTime Balancing Authority Mapping

### Current state

`WATTTIME_REGION = "PSCO"` is hardcoded. WattTime's free tier gives real-time CO2 percentile for all regions but restricts full signal data to CAISO_NORTH. We are currently using the free-tier CO2 percentile for PSCO, which works.

### How to map utility to balancing authority

The US has ~66 balancing authorities. The mapping from utility to BA is mostly static and well-documented:

| Source | Method | Cost |
|--------|--------|------|
| WattTime `/v3/region-from-loc` | Lat/lon to BA code | Free (all tiers) |
| DOE FEMP BA Lookup Tool | ZIP code to BA | Free (web tool, no documented API) |
| EIA-861 dataset | Utility ID to BA mapping (annual CSV) | Free |
| Static lookup table | We maintain it | Free |

### Recommendation for v1.0

**Static lookup in the TOU schedule DynamoDB table.**

Add a `watttime_region` field to the `evse-tou-schedules` table:

```
xcel_co / re_tou:
  ...
  watttime_region: "PSCO"
```

The scheduler Lambda reads the device's utility config and gets the WattTime region along with the TOU schedule. For 1-2 utilities, this is one field per record.

**When we expand**: Use WattTime's `/v3/region-from-loc` endpoint (free for all tiers) to auto-populate the BA code from the device's install coordinates. This can be a one-time call at device commissioning, stored in the device registry or the TOU config.

---

## 4. Multi-Utility Scheduler Architecture

### Current architecture (single device, hardcoded)

```
EventBridge (rate: 5 min)
  --> charge_scheduler_lambda.lambda_handler()
      --> is_tou_peak(now_mt)         # hardcoded Xcel CO
      --> get_moer_percent()          # hardcoded PSCO
      --> send_charge_command()       # single device via get_device_id()
```

### Proposed architecture (multi-device, per-device config)

```
EventBridge (rate: 5 min)
  --> charge_scheduler_lambda.lambda_handler()
      1. List active devices from device registry
         (DynamoDB scan/query: status="active")
      2. For each device:
         a. Read device config: utility_id, rate_plan from registry
         b. Read TOU schedule: peak hours, timezone, watttime_region
            from evse-tou-schedules table (cache per utility)
         c. Evaluate TOU: is_tou_peak(now, schedule)
         d. Evaluate MOER: get_moer_percent(watttime_region)
            (cache per region -- same BA for multiple devices)
         e. Decision: should_pause = tou_peak or moer_high
         f. Dedup: compare to last-sent state for this device
         g. Send downlink if changed: send_charge_command(device_id, ...)
      3. Log all decisions
```

### Key changes to `charge_scheduler_lambda.py`

1. **Remove hardcoded constants**: Delete `MT = ZoneInfo("America/Denver")`, `WATTTIME_REGION = "PSCO"`, and the hardcoded `is_tou_peak()` function.

2. **Add device iteration**: Replace `get_device_id()` (single device) with a query to the device registry for all `status="active"` devices.

3. **Add config lookup**: New function `get_device_schedule(device)` that reads the device's `utility_id` and `rate_plan`, then fetches the TOU config from `evse-tou-schedules`.

4. **Parameterize TOU check**: `is_tou_peak(now_utc, schedule)` takes a schedule dict with `timezone`, `peak_weekdays`, `peak_start_hour`, `peak_end_hour`, `peak_months`.

5. **Parameterize WattTime**: `get_moer_percent(region)` takes a BA code instead of using the global `WATTTIME_REGION`.

6. **Per-device state**: `get_last_state(device_id)` and `write_state(device_id, ...)` already take a device_id via `get_device_id()`. Change them to accept an explicit device_id parameter.

7. **Per-device downlink**: `send_charge_command(device_id, allowed)` takes an explicit device_id.

8. **Caching within invocation**: TOU schedules and MOER values are cached by utility_id and watttime_region respectively, so 10 devices on the same utility make 1 DynamoDB read and 1 WattTime API call, not 10.

### IAM changes

The scheduler Lambda needs read access to the device registry table (`evse-devices`) and the TOU schedules table (`evse-tou-schedules`). Add to Terraform.

### Scaling concern

At 10 devices, the Lambda iterates 10 times in sequence -- fine (each iteration is ~100ms for DynamoDB + WattTime, total <2s). At 1,000 devices, we would need to batch by utility/region and parallelize. That is not a v1.0 concern.

---

## 5. v1.0 Minimum Viable Scope

The first 10 installations are all in Colorado Springs on Xcel Energy (PSCO region, RE-TOU rate plan). The minimum viable scope must support this while laying the groundwork for multi-utility.

### What to build

| Item | Description | Effort |
|------|-------------|--------|
| **TOU schedules DynamoDB table** | `evse-tou-schedules`, Terraform-managed. Seed with Xcel CO RE-TOU data. | S |
| **Device registry `utility_id` + `rate_plan` fields** | Add to device registry schema (TASK-036 dependency). Default: `xcel_co` / `re_tou`. | S |
| **Scheduler reads per-device config** | Replace hardcoded TOU/MOER with lookup from registry + TOU table. Iterate over active devices. | M |
| **Per-device downlink routing** | `send_charge_command(device_id, ...)` instead of `get_device_id()` singleton. | S |
| **Cache TOU + MOER per invocation** | Same utility = same schedule lookup. Same BA = same MOER call. | S |
| **Terraform updates** | New DynamoDB table, IAM policy for scheduler to read both tables. | S |
| **Tests** | Update `aws/tests/` -- mock device registry, mock TOU table, test multi-device iteration, test TOU evaluation with different schedules. | M |

### What NOT to build for v1.0

- No OpenEI/USURDB API integration (manual TOU data entry is fine for 1-2 utilities)
- No automated meter-number-to-utility resolution
- No lat/lon-to-utility geospatial lookup
- No Arcadia/Genability subscription
- No utility selection dropdown in a mobile app (the commissioning tool is a CLI or simple form -- TASK-036 scope)
- No multi-rate-plan support per device (one rate plan per device is sufficient)

### Dependencies

- **TASK-036 (Device Registry)**: The scheduler needs a device registry table to iterate over active devices and read their utility config. TASK-037 cannot ship without TASK-036 or a temporary equivalent (e.g., a hardcoded device list in DynamoDB).
- If TASK-036 is not ready, a **stopgap** is a simple `device_config` DynamoDB table with `device_id` as the PK and `utility_id`, `rate_plan`, `watttime_region` as attributes. This is effectively a subset of the device registry and can be merged into it when TASK-036 ships.

### Rollout plan

1. Deploy TOU schedules table + seed Xcel data (Terraform apply)
2. Deploy updated scheduler Lambda (reads config, iterates devices)
3. Add the existing device (`b319d001-...`) to the device config with `utility_id=xcel_co`, `rate_plan=re_tou`
4. Verify: scheduler correctly reads config, evaluates TOU, sends downlink -- identical behavior to today
5. Add a second test device (if available) with a different config to verify multi-device iteration

---

## 6. Data Sources Summary

### Free / Open

| Source | What it provides | Access | Limitations |
|--------|-----------------|--------|-------------|
| **OpenEI USURDB** | 3,700+ US utility rate structures, TOU schedules as hourly arrays, demand/energy charges | Free REST API (`api.openei.org/utility_rates`), no API key required. Bulk CSV download also available. | Complex response format (designed for bill calc, not "is it peak now?"). Updated ~annually by NREL. ZIP code param deprecated Feb 2025; use lat/lon. |
| **NREL ZIP-to-Utility dataset** | ZIP code to utility name + EIA ID + average rates | Free CSV download from OEDI (EIA-861 based). Updated annually. | ~15% of ZIPs have multiple utilities. Static file, not a live API. |
| **EIA-861** | Utility service territories, utility-to-BA mapping, customer counts, sales data | Free annual CSV from eia.gov. | No real-time API. Annual release cadence. |
| **DOE FEMP BA Lookup** | ZIP code to balancing authority | Free web tool at energy.gov/femp. | Web form only -- no documented REST API. Manual use. |
| **WattTime (free tier)** | CO2 percentile (0-100) for all regions. `/v3/region-from-loc` (lat/lon to BA code) for all regions. | Free with registration. | Full signal data (MOER values, not just percentile) restricted to CAISO_NORTH on free tier. Analyst/Pro for other regions. Current SideCharge usage (CO2 percentile for PSCO) works on free tier. |
| **US Census Geocoder** | Street address to lat/lon | Free, no API key. | US addresses only. Batch processing available. |

### Paid / Commercial

| Source | What it provides | Pricing | Notes |
|--------|-----------------|---------|-------|
| **WattTime Analyst/Pro** | Full MOER signal (not just percentile), historical data, forecast, all regions | Custom pricing (contact sales). Analyst and Pro tiers. | We currently use the free-tier CO2 percentile, which is sufficient. If we need raw MOER values or forecasts, we need Analyst tier. |
| **Arcadia (formerly Genability) Signal API** | Utility identification by ZIP, tariff lookup, TOU schedules, on-demand cost calculations. Covers ~98% of US customers. | Custom pricing (contact sales). No public rate card. | The most comprehensive commercial option. TOU endpoint returns period definitions with peak/off-peak designations. Utility lookup by postal code. Overkill for <50 utilities but the right choice at scale. |
| **UtilityAPI** | Utility bill data access (Green Button Connect), real customer usage data | Tiered pricing (see utilityapi.com/pricing). | Focused on bill access, not rate schedule lookup. Useful for future energy analytics, not needed for TOU scheduling. |
| **Google Maps Geocoding API** | Address to lat/lon | Free tier: 28,000 requests/month. Beyond: $5/1,000 requests. | Only needed if we geocode install addresses. US Census Geocoder is a free alternative. |

### Recommendation

For v1.0 (1-2 utilities, 10 devices): **No external API subscriptions needed.** Manual TOU data entry into DynamoDB, WattTime free tier for MOER percentile, static BA mapping.

For v2.0 (10-50 utilities): **OpenEI USURDB** to bulk-populate TOU schedules into DynamoDB (free). **WattTime `/v3/region-from-loc`** to auto-map device coordinates to BA (free). Total cost: $0.

For v3.0 (50+ utilities, multiple rate plans per utility): **Evaluate Arcadia Signal API** for comprehensive tariff data and automated TOU extraction. Cost: TBD (custom pricing, likely $500-2,000/month based on industry benchmarks for similar data services).

---

## Open Questions

1. **Rate plan identification**: The meter number does not tell us which rate plan the customer is on (Xcel has R, RE-TOU, S-EV, etc.). For v1.0, we assume all Colorado Springs installs are on RE-TOU. For v2.0, we either ask the installer to confirm the rate plan or query the utility's API (Xcel has a customer API behind MyAccount login -- not easily automatable).

2. **TOU schedule changes**: Xcel changed peak hours from 2-7 PM to 5-9 PM in October 2024. How do we detect and propagate schedule changes? For v1.0: manual update to DynamoDB when we learn of a change. For v2.0: periodic USURDB re-import with diff detection.

3. **Seasonal vs. year-round TOU**: Xcel Colorado's RE-TOU is year-round, but many utilities have summer-only or winter-only peak periods. The DynamoDB schema should support `peak_months` from day one (included in the proposed schema above).

4. **TASK-036 dependency**: The multi-device scheduler needs a device registry to iterate. If TASK-036 is delayed, we can ship a stopgap `device_config` table with just the fields the scheduler needs.

5. **Flat-rate customers**: Some installations may be on flat-rate (non-TOU) plans. The scheduler should handle "no TOU schedule" gracefully -- skip TOU evaluation, only apply MOER-based delay windows.
