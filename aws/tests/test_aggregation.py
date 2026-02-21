"""Tests for the daily aggregation Lambda (TASK-078)."""

import json
import os
import sys
from datetime import datetime, timezone
from unittest.mock import MagicMock, patch

import pytest

# Ensure aws/ is on the path and boto3 is mocked before import
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
if "boto3" not in sys.modules:
    sys.modules["boto3"] = MagicMock()

import aggregation_lambda as agg


# ================================================================
# Constants
# ================================================================

# 2026-02-18 00:00:00 UTC in ms
DAY_START_MS = int(
    datetime(2026, 2, 18, tzinfo=timezone.utc).timestamp() * 1000
)
DAY_END_MS = DAY_START_MS + 86_400_000


# ================================================================
# Helpers
# ================================================================

def make_event(timestamp_ms, pilot_state="A", current_ma=0,
               cool_active=False, charge_allowed=True,
               fault_sensor=False, fault_clamp=False,
               fault_interlock=False, fault_selftest=False):
    """Build a mock telemetry event matching DynamoDB shape."""
    evse = {
        "pilot_state": pilot_state,
        "current_draw_ma": current_ma,
        "thermostat_cool_active": cool_active,
        "charge_allowed": charge_allowed,
    }
    if fault_sensor:
        evse["fault_sensor"] = True
    if fault_clamp:
        evse["fault_clamp"] = True
    if fault_interlock:
        evse["fault_interlock"] = True
    if fault_selftest:
        evse["fault_selftest"] = True
    return {
        "device_id": "test-wireless-id",
        "timestamp": timestamp_ms,
        "event_type": "evse_telemetry",
        "data": {"evse": evse},
    }


def ts(hour, minute=0):
    """Convert hour:minute on 2026-02-18 to ms timestamp."""
    return DAY_START_MS + (hour * 3600 + minute * 60) * 1000


# ================================================================
# compute_aggregates — zero events
# ================================================================

class TestZeroEvents:
    def test_zero_event_defaults(self):
        result = agg.compute_aggregates([], DAY_START_MS, DAY_END_MS)
        assert result["event_count"] == 0
        assert result["availability_pct"] == 0.0
        assert result["longest_gap_minutes"] == 1440
        assert result["total_kwh"] == 0.0
        assert result["charge_session_count"] == 0
        assert result["charge_duration_min"] == 0.0
        assert result["peak_current_ma"] == 0
        assert result["ac_compressor_hours"] == 0.0
        assert result["fault_sensor_count"] == 0
        assert result["fault_clamp_count"] == 0
        assert result["fault_interlock_count"] == 0
        assert result["selftest_failed"] is False


# ================================================================
# compute_aggregates — availability
# ================================================================

class TestAvailability:
    def test_single_event_availability(self):
        events = [make_event(ts(12))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["event_count"] == 1
        assert result["availability_pct"] == round(1 / 96 * 100, 1)

    def test_full_day_availability_caps_at_100(self):
        """More than 96 events should cap at 100%."""
        events = [make_event(DAY_START_MS + i * 60_000) for i in range(200)]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["availability_pct"] == 100.0

    def test_96_events_is_100_pct(self):
        events = [make_event(DAY_START_MS + i * 900_000) for i in range(96)]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["availability_pct"] == 100.0


# ================================================================
# compute_aggregates — longest gap
# ================================================================

class TestLongestGap:
    def test_single_event_at_noon(self):
        """Gap from midnight to noon (12h) and noon to midnight (12h)."""
        events = [make_event(ts(12))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["longest_gap_minutes"] == 720.0

    def test_gap_from_midnight_to_first(self):
        """First event at 06:00 → 6h gap from midnight."""
        events = [make_event(ts(6)), make_event(ts(7))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # Gaps: midnight→06:00 = 360min, 06:00→07:00 = 60min, 07:00→midnight = 1020min
        assert result["longest_gap_minutes"] == 1020.0

    def test_gap_from_last_to_midnight(self):
        """Last event at 23:00 → 1h gap to midnight."""
        events = [make_event(ts(0, 30)), make_event(ts(23))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # Gaps: midnight→00:30 = 30min, 00:30→23:00 = 1350min, 23:00→midnight = 60min
        assert result["longest_gap_minutes"] == 1350.0

    def test_evenly_spaced_events(self):
        """Every 15 min = 15 min max gap (plus midnight boundaries)."""
        events = [make_event(DAY_START_MS + i * 900_000) for i in range(96)]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # First event at 00:00, last at 23:45. Gaps all 15min.
        # Gap from 23:45 to midnight = 15min. Gap from midnight to 00:00 = 0min.
        assert result["longest_gap_minutes"] == 15.0

    def test_two_events_close_together(self):
        """Two events 1 min apart at noon — largest gap is midnight boundary."""
        events = [make_event(ts(12)), make_event(ts(12, 1))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # Gaps: 0→12:00 = 720, 12:00→12:01 = 1, 12:01→24:00 = 719
        assert result["longest_gap_minutes"] == 720.0


# ================================================================
# compute_aggregates — energy (kWh)
# ================================================================

class TestEnergy:
    def test_no_charging_no_kwh(self):
        """State A events produce no energy."""
        events = [make_event(ts(0)), make_event(ts(12))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["total_kwh"] == 0.0

    def test_charging_one_hour(self):
        """State C at 30A (30000mA) for 1 hour at 240V = 7.2 kWh."""
        events = [
            make_event(ts(10), pilot_state="C", current_ma=30000),
            make_event(ts(11), pilot_state="A", current_ma=0),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # 30A * 240V = 7200W * 1h = 7200Wh = 7.2kWh
        assert result["total_kwh"] == 7.2

    def test_charging_half_hour(self):
        """State C at 20A for 30 minutes at 240V = 2.4 kWh."""
        events = [
            make_event(ts(10), pilot_state="C", current_ma=20000),
            make_event(ts(10, 30), pilot_state="A", current_ma=0),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # 20A * 240V = 4800W * 0.5h = 2400Wh = 2.4kWh
        assert result["total_kwh"] == 2.4

    def test_state_c_no_current_no_energy(self):
        """State C but 0 current produces no energy (edge case)."""
        events = [
            make_event(ts(10), pilot_state="C", current_ma=0),
            make_event(ts(11), pilot_state="A"),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["total_kwh"] == 0.0

    def test_charging_extends_to_day_end(self):
        """Last event is State C — charging assumed until midnight."""
        events = [
            make_event(ts(22), pilot_state="C", current_ma=10000),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # 10A * 240V = 2400W * 2h (22:00 to midnight) = 4800Wh = 4.8kWh
        assert result["total_kwh"] == 4.8


# ================================================================
# compute_aggregates — charge sessions
# ================================================================

class TestChargeSessions:
    def test_one_session(self):
        events = [
            make_event(ts(8), pilot_state="B"),
            make_event(ts(9), pilot_state="C", current_ma=15000),
            make_event(ts(11), pilot_state="B"),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["charge_session_count"] == 1
        assert result["charge_duration_min"] == 120.0  # 09:00 to 11:00

    def test_two_sessions(self):
        events = [
            make_event(ts(8), pilot_state="C", current_ma=10000),
            make_event(ts(9), pilot_state="A"),
            make_event(ts(14), pilot_state="C", current_ma=10000),
            make_event(ts(16), pilot_state="A"),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["charge_session_count"] == 2
        # Session 1: 08-09 = 60min, Session 2: 14-16 = 120min
        assert result["charge_duration_min"] == 180.0

    def test_session_extends_to_day_end(self):
        """Charging at last event extends to midnight."""
        events = [
            make_event(ts(23), pilot_state="C", current_ma=10000),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["charge_session_count"] == 1
        assert result["charge_duration_min"] == 60.0  # 23:00 to midnight

    def test_no_sessions(self):
        events = [
            make_event(ts(8), pilot_state="A"),
            make_event(ts(12), pilot_state="B"),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["charge_session_count"] == 0
        assert result["charge_duration_min"] == 0.0


# ================================================================
# compute_aggregates — peak current
# ================================================================

class TestPeakCurrent:
    def test_peak_current(self):
        events = [
            make_event(ts(8), current_ma=5000),
            make_event(ts(9), current_ma=30000),
            make_event(ts(10), current_ma=15000),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["peak_current_ma"] == 30000

    def test_peak_current_zero_when_no_current(self):
        events = [make_event(ts(8)), make_event(ts(12))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["peak_current_ma"] == 0


# ================================================================
# compute_aggregates — AC compressor
# ================================================================

class TestACCompressor:
    def test_compressor_hours(self):
        events = [
            make_event(ts(10), cool_active=True),
            make_event(ts(12), cool_active=False),
            make_event(ts(14), cool_active=True),
            make_event(ts(15), cool_active=False),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # Cool from 10-12 (2h) and 14-15 (1h) = 3h
        assert result["ac_compressor_hours"] == 3.0

    def test_compressor_extends_to_day_end(self):
        """Compressor on at last event extends to midnight."""
        events = [
            make_event(ts(22), cool_active=True),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # 22:00 to midnight = 2h
        assert result["ac_compressor_hours"] == 2.0

    def test_no_compressor(self):
        events = [make_event(ts(8)), make_event(ts(12))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["ac_compressor_hours"] == 0.0


# ================================================================
# compute_aggregates — faults
# ================================================================

class TestFaults:
    def test_fault_sensor_count(self):
        events = [
            make_event(ts(8), fault_sensor=True),
            make_event(ts(9)),
            make_event(ts(10), fault_sensor=True),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["fault_sensor_count"] == 2

    def test_fault_clamp_count(self):
        events = [
            make_event(ts(8), fault_clamp=True),
            make_event(ts(9), fault_clamp=True),
            make_event(ts(10), fault_clamp=True),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["fault_clamp_count"] == 3

    def test_fault_interlock_count(self):
        events = [make_event(ts(8), fault_interlock=True)]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["fault_interlock_count"] == 1

    def test_selftest_failed_any_event(self):
        """selftest_failed is True if any event had it set."""
        events = [
            make_event(ts(8)),
            make_event(ts(9), fault_selftest=True),
            make_event(ts(10)),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["selftest_failed"] is True

    def test_selftest_not_failed(self):
        events = [make_event(ts(8)), make_event(ts(12))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["selftest_failed"] is False

    def test_multiple_fault_types_same_event(self):
        events = [
            make_event(ts(8), fault_sensor=True, fault_clamp=True,
                       fault_interlock=True, fault_selftest=True),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["fault_sensor_count"] == 1
        assert result["fault_clamp_count"] == 1
        assert result["fault_interlock_count"] == 1
        assert result["selftest_failed"] is True

    def test_no_faults(self):
        events = [make_event(ts(8))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["fault_sensor_count"] == 0
        assert result["fault_clamp_count"] == 0
        assert result["fault_interlock_count"] == 0
        assert result["selftest_failed"] is False


# ================================================================
# write_aggregate — TTL
# ================================================================

class TestWriteAggregate:
    def test_ttl_is_3_years_from_day_start(self):
        """TTL should be day start epoch + 94,608,000 seconds."""
        mock_table = MagicMock()
        with patch.object(agg, "aggregates_table", mock_table):
            agg.write_aggregate("SC-TEST", "wid-001", "2026-02-18", {
                "event_count": 5,
            })

        item = mock_table.put_item.call_args[1]["Item"]
        day_epoch = int(
            datetime(2026, 2, 18, tzinfo=timezone.utc).timestamp()
        )
        assert item["ttl"] == day_epoch + 94_608_000
        assert item["device_id"] == "SC-TEST"
        assert item["date"] == "2026-02-18"
        assert item["wireless_device_id"] == "wid-001"
        assert item["event_count"] == 5


# ================================================================
# lambda_handler
# ================================================================

class TestLambdaHandler:
    def test_handler_defaults_to_yesterday(self):
        """No 'date' in event → aggregates yesterday."""
        mock_devices = [
            {"device_id": "SC-AAA", "wireless_device_id": "wid-001",
             "status": "active"},
        ]
        mock_events = [
            make_event(DAY_START_MS + 3_600_000, pilot_state="A"),
        ]
        with patch.object(agg, "get_all_active_devices", return_value=mock_devices), \
             patch.object(agg, "query_device_events", return_value=mock_events), \
             patch.object(agg, "write_aggregate") as mock_write:
            result = agg.lambda_handler({}, None)

        assert result["statusCode"] == 200
        body = json.loads(result["body"])
        assert body["devices"] == 1
        assert body["errors"] == 0
        # Should have called write with some date string
        mock_write.assert_called_once()

    def test_handler_uses_date_override(self):
        """Explicit date in event payload for backfilling."""
        mock_devices = [
            {"device_id": "SC-BBB", "wireless_device_id": "wid-002",
             "status": "active"},
        ]
        with patch.object(agg, "get_all_active_devices", return_value=mock_devices), \
             patch.object(agg, "query_device_events", return_value=[]), \
             patch.object(agg, "write_aggregate") as mock_write:
            result = agg.lambda_handler({"date": "2026-01-15"}, None)

        body = json.loads(result["body"])
        assert body["date"] == "2026-01-15"
        assert body["devices"] == 1
        # Zero events → still writes a record (availability tracking)
        call_args = mock_write.call_args
        assert call_args[0][2] == "2026-01-15"  # date_str arg

    def test_handler_no_devices(self):
        with patch.object(agg, "get_all_active_devices", return_value=[]):
            result = agg.lambda_handler({}, None)
        assert result["statusCode"] == 200
        assert "No active devices" in result["body"]

    def test_handler_skips_device_without_wireless_id(self):
        mock_devices = [
            {"device_id": "SC-NOID", "status": "active"},
        ]
        with patch.object(agg, "get_all_active_devices", return_value=mock_devices), \
             patch.object(agg, "write_aggregate") as mock_write:
            result = agg.lambda_handler({"date": "2026-02-18"}, None)

        body = json.loads(result["body"])
        assert body["devices"] == 0
        mock_write.assert_not_called()

    def test_handler_catches_device_errors(self):
        mock_devices = [
            {"device_id": "SC-ERR", "wireless_device_id": "wid-bad",
             "status": "active"},
        ]
        with patch.object(agg, "get_all_active_devices", return_value=mock_devices), \
             patch.object(agg, "query_device_events",
                          side_effect=Exception("DynamoDB timeout")):
            result = agg.lambda_handler({"date": "2026-02-18"}, None)

        body = json.loads(result["body"])
        assert body["errors"] == 1
        assert body["devices"] == 0


# ================================================================
# compute_aggregates — single event edge case
# ================================================================

class TestSingleEvent:
    def test_single_event_charging(self):
        """One State C event — charging extends to day end."""
        events = [make_event(ts(20), pilot_state="C", current_ma=24000)]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        # 24A * 240V = 5760W * 4h (20:00-midnight) = 23040Wh = 23.04kWh
        assert result["total_kwh"] == 23.04
        assert result["charge_session_count"] == 1
        assert result["charge_duration_min"] == 240.0  # 4 hours
        assert result["peak_current_ma"] == 24000
        assert result["event_count"] == 1

    def test_single_idle_event(self):
        """One State A event — no energy, no sessions."""
        events = [make_event(ts(12))]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)
        assert result["total_kwh"] == 0.0
        assert result["charge_session_count"] == 0
        assert result["charge_duration_min"] == 0.0


# ================================================================
# compute_aggregates — mixed scenario
# ================================================================

class TestMixedScenario:
    def test_realistic_day(self):
        """Simulate a realistic day: idle morning, charge session, AC compressor,
        a fault, then idle evening."""
        events = [
            # Early morning idle
            make_event(ts(0, 15), pilot_state="A"),
            make_event(ts(6), pilot_state="A"),
            # Car plugged in, starts charging at 8am
            make_event(ts(8), pilot_state="B"),
            make_event(ts(8, 5), pilot_state="C", current_ma=32000),
            # Clamp fault during charging
            make_event(ts(9), pilot_state="C", current_ma=32000, fault_clamp=True),
            # Charging continues, fault clears
            make_event(ts(10), pilot_state="C", current_ma=32000),
            # Charge complete at 11am
            make_event(ts(11), pilot_state="B"),
            # Car unplugged
            make_event(ts(11, 30), pilot_state="A"),
            # AC compressor kicks on at 2pm
            make_event(ts(14), pilot_state="A", cool_active=True),
            make_event(ts(16), pilot_state="A", cool_active=False),
            # Evening idle
            make_event(ts(20), pilot_state="A"),
            make_event(ts(23, 45), pilot_state="A"),
        ]
        result = agg.compute_aggregates(events, DAY_START_MS, DAY_END_MS)

        assert result["event_count"] == 12
        assert result["charge_session_count"] == 1
        # Charging 08:05 to 11:00 = 175 min
        assert result["charge_duration_min"] == 175.0
        # 32A * 240V = 7680W for 175 min = 7680 * 175/60 / 1000 = 22.4 kWh
        assert result["total_kwh"] == 22.4
        assert result["peak_current_ma"] == 32000
        # AC 14:00 to 16:00 = 2h
        assert result["ac_compressor_hours"] == 2.0
        assert result["fault_clamp_count"] == 1
        assert result["fault_sensor_count"] == 0
        assert result["selftest_failed"] is False
