"""Tests for charge_scheduler_lambda.py — TOU schedule and charge decisions."""

import json
import os
import sys
from datetime import datetime
from unittest.mock import MagicMock, patch
from zoneinfo import ZoneInfo

import pytest

# Module-level mocking
mock_sidewalk_utils = MagicMock()
mock_sidewalk_utils.get_device_id.return_value = "test-device-id"
mock_sidewalk_utils.send_sidewalk_msg = MagicMock()
sys.modules["sidewalk_utils"] = mock_sidewalk_utils

if "boto3" not in sys.modules:
    sys.modules["boto3"] = MagicMock()

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import charge_scheduler_lambda as sched  # noqa: E402

MT = ZoneInfo("America/Denver")


# --- TOU schedule ---

class TestTouPeak:
    def test_weekday_5pm_is_peak(self):
        # Monday 5:00 PM MT
        dt = datetime(2025, 1, 6, 17, 0, tzinfo=MT)
        assert sched.is_tou_peak(dt) is True

    def test_weekday_8pm_is_peak(self):
        # Wednesday 8:59 PM MT
        dt = datetime(2025, 1, 8, 20, 59, tzinfo=MT)
        assert sched.is_tou_peak(dt) is True

    def test_weekday_9pm_is_off_peak(self):
        # Thursday 9:00 PM MT (on-peak ends at 9)
        dt = datetime(2025, 1, 9, 21, 0, tzinfo=MT)
        assert sched.is_tou_peak(dt) is False

    def test_weekday_4pm_is_off_peak(self):
        # Tuesday 4:59 PM MT
        dt = datetime(2025, 1, 7, 16, 59, tzinfo=MT)
        assert sched.is_tou_peak(dt) is False

    def test_weekend_5pm_is_off_peak(self):
        # Saturday 5:00 PM MT
        dt = datetime(2025, 1, 4, 17, 0, tzinfo=MT)
        assert sched.is_tou_peak(dt) is False

    def test_sunday_7pm_is_off_peak(self):
        dt = datetime(2025, 1, 5, 19, 0, tzinfo=MT)
        assert sched.is_tou_peak(dt) is False

    def test_weekday_morning_is_off_peak(self):
        dt = datetime(2025, 1, 6, 8, 0, tzinfo=MT)
        assert sched.is_tou_peak(dt) is False

    def test_weekday_noon_is_off_peak(self):
        dt = datetime(2025, 1, 6, 12, 0, tzinfo=MT)
        assert sched.is_tou_peak(dt) is False


# --- Charge command format ---

class TestSendChargeCommand:
    def test_allow_payload(self):
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        sched.send_charge_command(True)
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        assert payload == bytes([0x10, 0x01, 0x00, 0x00])

    def test_pause_payload(self):
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        sched.send_charge_command(False)
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        assert payload == bytes([0x10, 0x00, 0x00, 0x00])

    def test_transmit_mode_zero(self):
        """Charge commands use transmit_mode=0 (confirmed downlink)."""
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        sched.send_charge_command(True)
        assert mock_sidewalk_utils.send_sidewalk_msg.call_args[1]["transmit_mode"] == 0


# --- Lambda handler decision logic ---

class TestLambdaHandler:
    @pytest.fixture(autouse=True)
    def mock_deps(self):
        """Mock DynamoDB, WattTime, and sidewalk."""
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            yield

    def test_off_peak_sends_allow(self):
        # Monday 10 AM — off-peak, no MOER
        with patch.object(sched, "datetime") if hasattr(sched, "datetime") else patch("charge_scheduler_lambda.datetime") as mock_dt:
            now = datetime(2025, 1, 6, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                result = sched.lambda_handler({}, None)
        assert "allow" in result["body"]

    def test_peak_sends_pause(self):
        # Monday 6 PM — on-peak
        now = datetime(2025, 1, 6, 18, 0, tzinfo=MT)
        with patch("charge_scheduler_lambda.datetime") as mock_dt:
            mock_dt.now.return_value = now
            mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
            result = sched.lambda_handler({}, None)
        assert "pause" in result["body"]

    def test_no_change_skips_downlink(self):
        """If last command matches, don't re-send."""
        with patch.object(sched, "get_last_state",
                          return_value={"last_command": "allow"}):
            now = datetime(2025, 1, 6, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                result = sched.lambda_handler({}, None)
        assert "no change" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()


# --- MOER integration ---

class TestMoerDecision:
    def test_high_moer_causes_pause(self):
        """MOER above threshold should pause even off-peak."""
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=85):  # > 70 threshold
            now = datetime(2025, 1, 6, 10, 0, tzinfo=MT)  # off-peak
            with patch("charge_scheduler_lambda.datetime") as mock_dt:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                result = sched.lambda_handler({}, None)
        assert "pause" in result["body"]
        assert "moer" in result["body"]

    def test_low_moer_allows(self):
        """MOER below threshold off-peak should allow."""
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=30):
            now = datetime(2025, 1, 6, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                result = sched.lambda_handler({}, None)
        assert "allow" in result["body"]

    def test_moer_none_treated_as_low(self):
        """If WattTime is unavailable (None), don't pause for MOER."""
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            now = datetime(2025, 1, 6, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                result = sched.lambda_handler({}, None)
        assert "allow" in result["body"]
