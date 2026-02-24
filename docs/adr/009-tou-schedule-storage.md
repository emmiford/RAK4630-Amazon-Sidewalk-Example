# ADR-009: TOU schedule storage via DynamoDB table with static utility identification

## Status
Accepted

## Context

The charge scheduler Lambda hardcodes Xcel Energy Colorado's TOU schedule
(`is_tou_peak`: weekdays 5-9 PM MT) and a single WattTime balancing authority
(`WATTTIME_REGION = "PSCO"`). This works for one device in Colorado Springs but
cannot support devices at different utilities, rate plans, or balancing
authorities. Two related decisions were needed:

1. **How does the system learn a device's utility?**
2. **Where are TOU schedules stored?**

## Decision

### Utility identification: static list (v1.0), graduate to lat/lon lookup (v2.0)

The device registry (§8.7) stores `utility_id` and `rate_plan` per device. For
v1.0 (<10 devices, all Xcel Colorado), the installer selects from a short
hardcoded list in the commissioning tool. The first 10 installations are all in
one metro area on one utility — a comprehensive database is unnecessary.

When the fleet expands beyond Colorado (10+ utilities), graduate to lat/lon
lookup: the NREL Utility Rates API (`api.openei.org/utility_rates`) and
WattTime's `/v3/region-from-loc` endpoint accept coordinates and return matching
utilities and balancing authorities. The device registry already collects
`install_address` (and optionally `install_lat`/`install_lon`), so this is a
backend-only change.

### TOU schedule storage: DynamoDB table

A new `evse-tou-schedules` DynamoDB table (PK: `utility_id`, SK: `rate_plan`)
stores TOU peak windows, timezone, and WattTime region per utility/rate
combination. The scheduler reads the device's `utility_id` from the registry,
fetches the schedule from this table, and evaluates per-device.

## Consequences

**Easier**:
- Adding a new utility is a DynamoDB put_item, not a code deploy
- Rate plan changes (e.g., Xcel changed peak hours in Oct 2024) are data
  updates, not Lambda redeploys
- The scheduler can iterate over all active devices with per-device TOU logic
- WattTime region is co-located with the TOU schedule — one lookup per utility

**Harder**:
- Someone must manually enter TOU data for each new utility (acceptable at <50
  utilities; mitigated by OpenEI bulk import at scale)
- An additional DynamoDB table to maintain

## Alternatives Considered

### Utility identification

**(a) ZIP code to utility lookup** (NREL/EIA-861 dataset): ~15% of US ZIP codes
are served by multiple utilities. Would require disambiguation. Rejected as
primary method but useful as a filter for the installer dropdown.

**(b) Meter number lookup**: Unambiguous but no national API exists to map meter
numbers to utilities. Each utility has proprietary formats. Meter number is
collected for future use but not used for utility identification in v1.0.

**(d) Lat/lon geospatial lookup** (NREL API + WattTime API): Fully automatic,
most accurate (uses service territory boundaries, not ZIP approximation).
Selected as the v2.0 graduation path when fleet expands beyond Colorado.

### TOU schedule storage

**(a) Hardcoded in Lambda code**: Current state. Works for 1-2 utilities. At 5+
utilities the Lambda becomes a mess of conditionals. Rate plan changes require
code deploys. Rejected for anything beyond prototype.

**(c) OpenEI USURDB real-time API**: Comprehensive (3,700+ utilities) but the
response format is designed for bill calculation, not "is it peak now?" queries.
Requires complex parsing of period indices to rate tiers. API is a lookup, not a
real-time service — results would need caching. Rejected as a real-time query
source, but recommended as the future data source for bulk-populating the
DynamoDB table at 10+ utilities.
