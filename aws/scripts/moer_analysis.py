#!/usr/bin/env python3
"""
MOER Threshold Analysis for PSCO Region

Pulls historical MOER data from WattTime API and analyzes the distribution
to determine the optimal charging pause threshold.

Usage:
    python3 aws/scripts/moer_analysis.py

Requires WATTTIME_USERNAME and WATTTIME_PASSWORD environment variables,
or reads from aws/terraform/terraform.tfvars.
"""

import os
import sys
from datetime import datetime, timedelta, timezone

import requests

# --- Configuration ---
REGION = "PSCO"  # Public Service Company of Colorado
SIGNAL_TYPE = "co2_moer"
DAYS_TO_FETCH = 30
THRESHOLDS = [50, 60, 70, 80, 90]  # Percent thresholds to analyze

# TOU peak hours (Xcel Energy Colorado)
TOU_PEAK_START = 15  # 3 PM MT (converted to UTC: 21 or 22 depending on DST)
TOU_PEAK_END = 19    # 7 PM MT
TOU_PEAK_DAYS = range(0, 5)  # Monday–Friday


def get_credentials():
    """Get WattTime credentials from env vars or terraform.tfvars."""
    username = os.environ.get("WATTTIME_USERNAME")
    password = os.environ.get("WATTTIME_PASSWORD")

    if username and password:
        return username, password

    # Try terraform.tfvars
    tfvars_path = os.path.join(os.path.dirname(__file__), "..", "terraform", "terraform.tfvars")
    if os.path.exists(tfvars_path):
        with open(tfvars_path) as f:
            for line in f:
                line = line.strip()
                if line.startswith("watttime_username"):
                    username = line.split("=", 1)[1].strip().strip('"')
                elif line.startswith("watttime_password"):
                    password = line.split("=", 1)[1].strip().strip('"')
        if username and password:
            return username, password

    print("Error: WattTime credentials not found.", file=sys.stderr)
    print("Set WATTTIME_USERNAME and WATTTIME_PASSWORD, or check terraform.tfvars", file=sys.stderr)
    sys.exit(1)


def watttime_login(username, password):
    """Authenticate with WattTime API and return bearer token."""
    resp = requests.get(
        "https://api.watttime.org/login",
        auth=(username, password),
        timeout=10,
    )
    resp.raise_for_status()
    return resp.json()["token"]


def fetch_moer_data(token, start, end):
    """Fetch MOER signal index data for the given time range."""
    headers = {"Authorization": f"Bearer {token}"}
    params = {
        "region": REGION,
        "signal_type": SIGNAL_TYPE,
        "start": start.isoformat(),
        "end": end.isoformat(),
    }
    resp = requests.get(
        "https://api.watttime.org/v3/signal-index",
        headers=headers,
        params=params,
        timeout=30,
    )
    resp.raise_for_status()
    return resp.json()


def is_tou_peak(dt):
    """Check if a datetime (UTC) falls in Xcel TOU peak hours (MT)."""
    # Approximate MT as UTC-7 (ignoring DST for simplicity)
    mt_hour = (dt.hour - 7) % 24
    mt_weekday = dt.weekday()
    return mt_weekday in TOU_PEAK_DAYS and TOU_PEAK_START <= mt_hour < TOU_PEAK_END


def analyze(data_points):
    """Analyze MOER distribution and threshold impact."""
    if not data_points:
        print("No data points to analyze.")
        return

    values = [p["value"] for p in data_points]
    n = len(values)

    print(f"\n{'='*60}")
    print(f"MOER Distribution Analysis — {REGION}")
    print(f"{'='*60}")
    print(f"Data points: {n}")
    print(f"Period: {data_points[-1]['point_time']} to {data_points[0]['point_time']}")
    print(f"Min: {min(values):.1f}%")
    print(f"Max: {max(values):.1f}%")
    print(f"Mean: {sum(values)/n:.1f}%")
    sorted_vals = sorted(values)
    print(f"Median: {sorted_vals[n//2]:.1f}%")
    print(f"P25: {sorted_vals[n//4]:.1f}%")
    print(f"P75: {sorted_vals[3*n//4]:.1f}%")

    # Threshold analysis
    print(f"\n{'='*60}")
    print("Threshold Analysis")
    print(f"{'='*60}")
    print(f"{'Threshold':>10} {'% Time Above':>14} {'Hours/Day Paused':>18} {'Recommendation':>16}")
    print(f"{'-'*10:>10} {'-'*14:>14} {'-'*18:>18} {'-'*16:>16}")

    # Assume 5-minute intervals
    interval_hours = 5 / 60  # WattTime typically reports every 5 min
    total_hours = n * interval_hours

    for threshold in THRESHOLDS:
        above = sum(1 for v in values if v > threshold)
        pct_above = 100.0 * above / n
        hours_paused_per_day = 24 * pct_above / 100

        if pct_above > 60:
            rec = "Too aggressive"
        elif pct_above > 40:
            rec = "Aggressive"
        elif pct_above > 20:
            rec = "Moderate"
        elif pct_above > 5:
            rec = "Conservative"
        else:
            rec = "Minimal impact"

        print(f"{threshold:>9}% {pct_above:>13.1f}% {hours_paused_per_day:>17.1f}h {rec:>16}")

    # TOU overlap analysis
    print(f"\n{'='*60}")
    print("TOU Peak Overlap (Xcel 3-7 PM MT weekdays)")
    print(f"{'='*60}")

    tou_points = []
    non_tou_points = []
    for p in data_points:
        try:
            dt = datetime.fromisoformat(p["point_time"].replace("Z", "+00:00"))
            if is_tou_peak(dt):
                tou_points.append(p["value"])
            else:
                non_tou_points.append(p["value"])
        except (ValueError, KeyError):
            continue

    if tou_points:
        tou_mean = sum(tou_points) / len(tou_points)
        non_tou_mean = sum(non_tou_points) / len(non_tou_points) if non_tou_points else 0
        print(f"TOU peak MOER mean: {tou_mean:.1f}% ({len(tou_points)} points)")
        print(f"Off-peak MOER mean: {non_tou_mean:.1f}% ({len(non_tou_points)} points)")
        print(f"Delta: {tou_mean - non_tou_mean:+.1f}%")

        # How often is MOER high during TOU peak?
        for threshold in [60, 70, 80]:
            tou_above = sum(1 for v in tou_points if v > threshold)
            tou_pct = 100.0 * tou_above / len(tou_points)
            print(f"  MOER > {threshold}% during TOU peak: {tou_pct:.1f}% of the time")

    # Recommendation
    print(f"\n{'='*60}")
    print("Recommendation")
    print(f"{'='*60}")

    # Find threshold where pause time is 4-8 hours/day (reasonable for overnight charging)
    best = None
    for threshold in THRESHOLDS:
        above = sum(1 for v in values if v > threshold)
        hours = 24 * above / n
        if 2 <= hours <= 8:
            best = threshold
            break

    if best:
        above = sum(1 for v in values if v > best)
        hours = 24 * above / n
        print(f"Suggested threshold: {best}%")
        print(f"  Pauses charging ~{hours:.1f} hours/day")
        print(f"  Targets the dirtiest {100*above/n:.0f}% of grid hours")
    else:
        print("Current threshold of 70% is a reasonable default.")
        print("Adjust based on the distribution above.")


def main():
    username, password = get_credentials()
    print(f"Authenticating as {username}...")
    token = watttime_login(username, password)
    print("Authenticated.")

    end = datetime.now(timezone.utc)
    start = end - timedelta(days=DAYS_TO_FETCH)

    print(f"Fetching {DAYS_TO_FETCH} days of MOER data for {REGION}...")

    all_data = []
    # WattTime may limit response size; fetch in chunks
    chunk_start = start
    while chunk_start < end:
        chunk_end = min(chunk_start + timedelta(days=7), end)
        try:
            result = fetch_moer_data(token, chunk_start, chunk_end)
            points = result.get("data", [])
            all_data.extend(points)
            print(f"  {chunk_start.date()} to {chunk_end.date()}: {len(points)} points")
        except requests.HTTPError as e:
            print(f"  {chunk_start.date()} to {chunk_end.date()}: HTTP {e.response.status_code}")
            if e.response.status_code == 401:
                print("  Re-authenticating...")
                token = watttime_login(username, password)
        chunk_start = chunk_end

    print(f"\nTotal data points: {len(all_data)}")

    if not all_data:
        print("No data retrieved. Check credentials and region.", file=sys.stderr)
        sys.exit(1)

    analyze(all_data)


if __name__ == "__main__":
    main()
