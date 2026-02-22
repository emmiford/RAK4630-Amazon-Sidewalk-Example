"""Tests for charge_scheduler_lambda.py — TOU schedule, delay windows, and charge decisions."""

import os
import struct
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


# --- TOU peak end calculation ---

class TestTouPeakEnd:
    def test_peak_end_is_9pm_today(self):
        now = datetime(2026, 2, 16, 18, 30, tzinfo=MT)  # Monday 6:30 PM
        end_sc = sched.get_tou_peak_end_sc(now)
        # 9 PM MT today as device epoch
        expected_9pm = now.replace(hour=21, minute=0, second=0, microsecond=0)
        expected_sc = int(expected_9pm.timestamp()) - sched.EPOCH_OFFSET
        assert end_sc == expected_sc

    def test_peak_end_at_5pm_still_9pm(self):
        now = datetime(2026, 2, 16, 17, 0, tzinfo=MT)  # Monday 5 PM
        end_sc = sched.get_tou_peak_end_sc(now)
        expected_9pm = now.replace(hour=21, minute=0, second=0, microsecond=0)
        expected_sc = int(expected_9pm.timestamp()) - sched.EPOCH_OFFSET
        assert end_sc == expected_sc


# --- Legacy charge command format ---

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

    def test_transmit_mode_reliable(self):
        """Charge commands use transmit_mode=1 (reliable delivery)."""
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        sched.send_charge_command(True)
        assert mock_sidewalk_utils.send_sidewalk_msg.call_args[1]["transmit_mode"] == 1


# --- Delay window downlink format ---

class TestSendDelayWindow:
    def test_window_payload_format(self):
        """10-byte payload: [0x10, 0x02, start_le_4B, end_le_4B]."""
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        sched.send_delay_window(1000, 2000)
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        assert len(payload) == 10
        assert payload[0] == 0x10  # CHARGE_CONTROL_CMD
        assert payload[1] == 0x02  # DELAY_WINDOW_SUBTYPE
        start = struct.unpack_from("<I", payload, 2)[0]
        end = struct.unpack_from("<I", payload, 6)[0]
        assert start == 1000
        assert end == 2000

    def test_window_transmit_mode_reliable(self):
        """Delay window commands use transmit_mode=1 (reliable delivery)."""
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        sched.send_delay_window(1000, 2000)
        assert mock_sidewalk_utils.send_sidewalk_msg.call_args[1]["transmit_mode"] == 1

    def test_window_large_epoch_values(self):
        """device epoch values can be large 32-bit numbers."""
        mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
        sched.send_delay_window(4000000, 4014400)  # ~46 days + 4 hours
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        start = struct.unpack_from("<I", payload, 2)[0]
        end = struct.unpack_from("<I", payload, 6)[0]
        assert start == 4000000
        assert end == 4014400


# --- Lambda handler: delay window decision logic ---

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

    def test_peak_sends_delay_window(self):
        """On-peak should send a delay window, not a legacy command."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)  # Monday 6 PM
        with patch("charge_scheduler_lambda.datetime") as mock_dt, \
             patch("charge_scheduler_lambda.time") as mock_time:
            mock_dt.now.return_value = now
            mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
            mock_time.time.return_value = now.timestamp()
            result = sched.lambda_handler({}, None)
        assert "delay_window" in result["body"]
        # Should have sent a 10-byte delay window payload
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        assert len(payload) == 10
        assert payload[0] == 0x10
        assert payload[1] == 0x02

    def test_off_peak_no_downlink(self):
        """Off-peak with no prior window should not send anything."""
        now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)  # Monday 10 AM
        with patch("charge_scheduler_lambda.datetime") as mock_dt, \
             patch("charge_scheduler_lambda.time") as mock_time:
            mock_dt.now.return_value = now
            mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
            mock_time.time.return_value = now.timestamp()
            result = sched.lambda_handler({}, None)
        assert "off_peak" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()

    def test_off_peak_cancels_active_window(self):
        """Off-peak after a delay window should send legacy allow to cancel."""
        with patch.object(sched, "get_last_state",
                          return_value={"last_command": "delay_window"}):
            now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "allow" in result["body"]
        # Should be a 4-byte legacy allow
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        assert payload == bytes([0x10, 0x01, 0x00, 0x00])

    def test_tou_window_end_is_9pm(self):
        """TOU delay window should end at 9 PM MT."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)
        with patch("charge_scheduler_lambda.datetime") as mock_dt, \
             patch("charge_scheduler_lambda.time") as mock_time:
            mock_dt.now.return_value = now
            mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
            mock_time.time.return_value = now.timestamp()
            sched.lambda_handler({}, None)
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        end_sc = struct.unpack_from("<I", payload, 6)[0]
        expected_9pm = now.replace(hour=21, minute=0, second=0, microsecond=0)
        expected_end_sc = int(expected_9pm.timestamp()) - sched.EPOCH_OFFSET
        assert end_sc == expected_end_sc


# --- Heartbeat re-send logic ---

class TestHeartbeat:
    def test_recent_same_window_skips(self):
        """If same window was sent <30 min ago, skip."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)
        now_unix = int(now.timestamp())
        now_sc = now_unix - sched.EPOCH_OFFSET
        peak_end_sc = sched.get_tou_peak_end_sc(now)

        last_state = {
            "last_command": "delay_window",
            "window_end_sc": peak_end_sc,
            "sent_unix": now_unix - 300,  # 5 min ago
        }
        with patch.object(sched, "get_last_state", return_value=last_state), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "no change" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()

    def test_stale_window_resends(self):
        """If same window was sent >30 min ago, re-send."""
        now = datetime(2026, 2, 16, 18, 30, tzinfo=MT)
        now_unix = int(now.timestamp())
        peak_end_sc = sched.get_tou_peak_end_sc(now)

        last_state = {
            "last_command": "delay_window",
            "window_end_sc": peak_end_sc,
            "sent_unix": now_unix - 2000,  # ~33 min ago
        }
        with patch.object(sched, "get_last_state", return_value=last_state), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "delay_window" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_called_once()


# --- MOER integration ---

class TestMoerDecision:
    def test_high_moer_sends_delay_window(self):
        """MOER above threshold should send a delay window."""
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=85):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)  # off-peak
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "delay_window" in result["body"]
        assert "moer" in result["body"]
        # MOER window should be 30 min from now
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        start_sc = struct.unpack_from("<I", payload, 2)[0]
        end_sc = struct.unpack_from("<I", payload, 6)[0]
        assert end_sc - start_sc == sched.MOER_WINDOW_DURATION_S

    def test_low_moer_off_peak_no_downlink(self):
        """MOER below threshold off-peak should not send anything."""
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=30):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "off_peak" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()

    def test_moer_none_treated_as_low(self):
        """If WattTime is unavailable (None), don't pause for MOER."""
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "off_peak" in result["body"]

    def test_tou_plus_moer_uses_longer_window(self):
        """When both TOU and MOER are active, use the longer window end."""
        with patch.object(sched, "get_last_state", return_value=None), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=85):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            # On-peak + high MOER
            now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "delay_window" in result["body"]
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        end_sc = struct.unpack_from("<I", payload, 6)[0]
        # TOU ends at 9 PM (3 hours = 10800s), MOER window is 30 min (1800s)
        # TOU end should be longer
        tou_end = sched.get_tou_peak_end_sc(now)
        assert end_sc == tou_end


# --- Force re-send (divergence detection, TASK-071) ---

class TestForceResend:
    def test_force_resend_bypasses_heartbeat_dedup(self):
        """force_resend=True should send delay window even if recently sent."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)
        now_unix = int(now.timestamp())
        peak_end_sc = sched.get_tou_peak_end_sc(now)

        last_state = {
            "last_command": "delay_window",
            "window_end_sc": peak_end_sc,
            "sent_unix": now_unix - 60,  # 1 min ago (normally would skip)
        }
        with patch.object(sched, "get_last_state", return_value=last_state), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({"force_resend": True}, None)
        assert "delay_window" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_called_once()

    def test_force_resend_off_peak_sends_allow(self):
        """force_resend=True off-peak with sentinel=allow re-sends allow."""
        last_state = {"last_command": "allow"}
        with patch.object(sched, "get_last_state", return_value=last_state), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)  # off-peak
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({"force_resend": True}, None)
        assert "allow" in result["body"]
        payload = mock_sidewalk_utils.send_sidewalk_msg.call_args[0][0]
        assert payload == bytes([0x10, 0x01, 0x00, 0x00])

    def test_normal_off_peak_allow_sentinel_no_resend(self):
        """Without force_resend, off-peak with sentinel=allow does NOT re-send."""
        last_state = {"last_command": "allow"}
        with patch.object(sched, "get_last_state", return_value=last_state), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)
        assert "off_peak" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()


# --- Charge Now opt-out guard (TASK-064 / ADR-003) ---

class TestChargeNowOptOut:
    """Tests for the Charge Now opt-out guard in the scheduler."""

    def test_optout_suppresses_peak_pause(self):
        """During TOU peak, if override_until is in the future, suppress pause."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)  # Monday 6 PM
        now_unix = int(now.timestamp())
        override_until = now_unix + 3600  # 1 hour from now (still in peak)

        sentinel = {
            "last_command": "delay_window",
            "charge_now_override_until": override_until,
        }
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state") as mock_write, \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)

        assert "charge_now_optout" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()
        # write_state should preserve override_until
        mock_write.assert_called_once()
        call_kwargs = mock_write.call_args
        assert call_kwargs[1].get("charge_now_override_until") == override_until

    def test_optout_resumes_after_expiry(self):
        """When override_until is in the past, normal scheduling resumes."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)  # Monday 6 PM
        now_unix = int(now.timestamp())
        override_until = now_unix - 60  # Expired 1 minute ago

        sentinel = {
            "last_command": "delay_window",
            "charge_now_override_until": override_until,
        }
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)

        # Should send delay_window normally (not suppressed)
        assert "delay_window" in result["body"]
        assert "charge_now_optout" not in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_called_once()

    def test_optout_suppresses_moer_pause(self):
        """High MOER with active opt-out should also be suppressed."""
        now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)  # Monday 10 AM off-peak
        now_unix = int(now.timestamp())
        override_until = now_unix + 3600

        sentinel = {
            "last_command": "delay_window",
            "charge_now_override_until": override_until,
        }
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=85):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)

        assert "charge_now_optout" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()

    def test_optout_does_not_suppress_off_peak_allow(self):
        """Off-peak allow should still be sent even if opt-out is active."""
        now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)  # off-peak
        now_unix = int(now.timestamp())
        override_until = now_unix + 3600  # still active

        sentinel = {
            "last_command": "delay_window",
            "charge_now_override_until": override_until,
        }
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)

        # Off-peak with delay_window → should send legacy allow
        assert "allow" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_called_once()

    def test_optout_suppresses_heartbeat_resend(self):
        """Heartbeat re-send during peak should be suppressed by opt-out."""
        now = datetime(2026, 2, 16, 18, 30, tzinfo=MT)
        now_unix = int(now.timestamp())
        override_until = now_unix + 1800  # still active

        sentinel = {
            "last_command": "delay_window",
            "window_end_sc": sched.get_tou_peak_end_sc(now),
            "sent_unix": now_unix - 2000,  # stale — would normally re-send
            "charge_now_override_until": override_until,
        }
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)

        # Opt-out takes precedence over heartbeat
        assert "charge_now_optout" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_not_called()

    def test_no_override_field_normal_behavior(self):
        """Sentinel without charge_now_override_until → normal scheduling."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)
        sentinel = {"last_command": "off_peak"}  # no override field
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state"), \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                result = sched.lambda_handler({}, None)

        assert "delay_window" in result["body"]
        mock_sidewalk_utils.send_sidewalk_msg.assert_called_once()

    def test_override_preserved_across_write_state(self):
        """write_state should include charge_now_override_until when active."""
        now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)  # off-peak
        now_unix = int(now.timestamp())
        override_until = now_unix + 3600

        sentinel = {
            "last_command": "off_peak",
            "charge_now_override_until": override_until,
        }
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state") as mock_write, \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                sched.lambda_handler({}, None)

        # write_state should carry the override_until through
        call_kwargs = mock_write.call_args
        assert call_kwargs[1].get("charge_now_override_until") == override_until

    def test_expired_override_not_preserved(self):
        """Expired override_until should NOT be carried through."""
        now = datetime(2026, 2, 16, 10, 0, tzinfo=MT)  # off-peak
        now_unix = int(now.timestamp())
        override_until = now_unix - 60  # expired

        sentinel = {
            "last_command": "off_peak",
            "charge_now_override_until": override_until,
        }
        with patch.object(sched, "get_last_state", return_value=sentinel), \
             patch.object(sched, "write_state") as mock_write, \
             patch.object(sched, "log_command_event"), \
             patch.object(sched, "get_moer_percent", return_value=None):
            with patch("charge_scheduler_lambda.datetime") as mock_dt, \
                 patch("charge_scheduler_lambda.time") as mock_time:
                mock_dt.now.return_value = now
                mock_dt.side_effect = lambda *a, **kw: datetime(*a, **kw)
                mock_time.time.return_value = now.timestamp()
                sched.lambda_handler({}, None)

        call_kwargs = mock_write.call_args
        assert call_kwargs[1].get("charge_now_override_until") is None


# --- DynamoDB state layer (new schema: ADR-006) ---

SC_TEST_ID = "SC-7C810AC9"  # generate_sc_short_id("test-device-id")


class TestGetScId:
    """_get_sc_id() delegates to device_registry.generate_sc_short_id."""

    def test_returns_sc_id_for_device(self):
        result = sched._get_sc_id()
        assert result == SC_TEST_ID

    def test_uses_sidewalk_utils_device_id(self):
        """Passes get_device_id() result to generate_sc_short_id."""
        mock_gen = MagicMock(return_value="SC-AAAABBBB")
        with patch.object(sched, "get_device_id", return_value="other-id"), \
             patch.object(sched.device_registry, "generate_sc_short_id", mock_gen):
            result = sched._get_sc_id()
        mock_gen.assert_called_once_with("other-id")
        assert result == "SC-AAAABBBB"


class TestGetLastState:
    """get_last_state() reads from state_table with scheduler_* field mapping."""

    def test_reads_from_state_table_with_sc_id(self):
        """Should call state_table.get_item(Key={"device_id": SC-ID})."""
        mock_state_tbl = MagicMock()
        mock_state_tbl.get_item.return_value = {"Item": {
            "device_id": SC_TEST_ID,
            "scheduler_last_command": "delay_window",
            "scheduler_reason": "tou_peak",
            "scheduler_moer_percent": "N/A",
            "scheduler_tou_peak": True,
            "scheduler_window_start_sc": 1000,
            "scheduler_window_end_sc": 2000,
            "scheduler_sent_unix": 1700000000,
            "charge_now_override_until": None,
        }}
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            result = sched.get_last_state()

        mock_state_tbl.get_item.assert_called_once_with(
            Key={"device_id": SC_TEST_ID}
        )
        assert result["last_command"] == "delay_window"
        assert result["reason"] == "tou_peak"
        assert result["moer_percent"] == "N/A"
        assert result["tou_peak"] is True
        assert result["window_start_sc"] == 1000
        assert result["window_end_sc"] == 2000
        assert result["sent_unix"] == 1700000000

    def test_returns_none_when_item_missing(self):
        """No item in DynamoDB → returns None."""
        mock_state_tbl = MagicMock()
        mock_state_tbl.get_item.return_value = {}
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            result = sched.get_last_state()
        assert result is None

    def test_returns_none_on_exception(self):
        """DynamoDB exception → returns None (logged)."""
        mock_state_tbl = MagicMock()
        mock_state_tbl.get_item.side_effect = Exception("DynamoDB timeout")
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            result = sched.get_last_state()
        assert result is None

    def test_maps_charge_now_override(self):
        """charge_now_override_until field is mapped correctly."""
        mock_state_tbl = MagicMock()
        mock_state_tbl.get_item.return_value = {"Item": {
            "device_id": SC_TEST_ID,
            "scheduler_last_command": "allow",
            "charge_now_override_until": 1700003600,
        }}
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            result = sched.get_last_state()
        assert result["charge_now_override_until"] == 1700003600

    def test_missing_fields_return_none(self):
        """Sparse item returns None for missing scheduler_* fields."""
        mock_state_tbl = MagicMock()
        mock_state_tbl.get_item.return_value = {"Item": {
            "device_id": SC_TEST_ID,
            "scheduler_last_command": "off_peak",
        }}
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            result = sched.get_last_state()
        assert result["last_command"] == "off_peak"
        assert result["reason"] is None
        assert result["window_start_sc"] is None
        assert result["window_end_sc"] is None
        assert result["sent_unix"] is None


class TestWriteState:
    """write_state() uses state_table.update_item with scheduler_* fields."""

    def test_update_item_called_with_sc_id_key(self):
        """Should call state_table.update_item(Key={"device_id": SC-ID})."""
        mock_state_tbl = MagicMock()
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            sched.write_state("delay_window", "tou_peak", None, True,
                              window_start_sc=1000, window_end_sc=2000,
                              sent_unix=1700000000)
        mock_state_tbl.update_item.assert_called_once()
        call_kwargs = mock_state_tbl.update_item.call_args[1]
        assert call_kwargs["Key"] == {"device_id": SC_TEST_ID}

    def test_update_expression_has_scheduler_prefix(self):
        """UpdateExpression should use scheduler_* field names."""
        mock_state_tbl = MagicMock()
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            sched.write_state("allow", "off_peak", 30, False)
        call_kwargs = mock_state_tbl.update_item.call_args[1]
        expr = call_kwargs["UpdateExpression"]
        assert "scheduler_last_command" in expr
        assert "scheduler_reason" in expr
        assert "scheduler_moer_percent" in expr
        assert "scheduler_tou_peak" in expr
        assert "scheduler_updated_at" in expr

    def test_optional_window_fields_in_expression(self):
        """Window fields only appear in expression when provided."""
        mock_state_tbl = MagicMock()
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            sched.write_state("delay_window", "tou_peak", None, True,
                              window_start_sc=100, window_end_sc=200,
                              sent_unix=1700000000)
        call_kwargs = mock_state_tbl.update_item.call_args[1]
        expr = call_kwargs["UpdateExpression"]
        assert "scheduler_window_start_sc" in expr
        assert "scheduler_window_end_sc" in expr
        assert "scheduler_sent_unix" in expr

    def test_optional_fields_omitted_when_none(self):
        """Window fields should NOT appear when not provided."""
        mock_state_tbl = MagicMock()
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            sched.write_state("off_peak", "off_peak", None, False)
        call_kwargs = mock_state_tbl.update_item.call_args[1]
        expr = call_kwargs["UpdateExpression"]
        assert "scheduler_window_start_sc" not in expr
        assert "scheduler_window_end_sc" not in expr
        assert "scheduler_sent_unix" not in expr

    def test_charge_now_override_in_expression(self):
        """charge_now_override_until appears when provided."""
        mock_state_tbl = MagicMock()
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            sched.write_state("charge_now_optout", "tou_peak", None, True,
                              charge_now_override_until=1700003600)
        call_kwargs = mock_state_tbl.update_item.call_args[1]
        expr = call_kwargs["UpdateExpression"]
        assert "charge_now_override_until" in expr

    def test_expression_values_contain_command(self):
        """ExpressionAttributeValues should include :cmd with the command."""
        mock_state_tbl = MagicMock()
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            sched.write_state("allow", "off_peak", 30, False)
        call_kwargs = mock_state_tbl.update_item.call_args[1]
        values = call_kwargs["ExpressionAttributeValues"]
        assert values[":cmd"] == "allow"
        assert values[":reason"] == "off_peak"

    def test_does_not_use_put_item(self):
        """write_state must NOT use put_item (old schema)."""
        mock_state_tbl = MagicMock()
        with patch.object(sched, "state_table", mock_state_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID):
            sched.write_state("allow", "off_peak", None, False)
        mock_state_tbl.put_item.assert_not_called()


class TestLogCommandEvent:
    """log_command_event() writes to events table with SC-ID and timestamp_mt."""

    def test_put_item_uses_sc_id(self):
        """device_id should be SC-ID, not wireless device ID."""
        mock_tbl = MagicMock()
        with patch.object(sched, "table", mock_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID),              patch("charge_scheduler_lambda.time") as mock_time:
            mock_time.time.return_value = 1700000000.0
            sched.log_command_event("allow", "off_peak", None, False)
        item = mock_tbl.put_item.call_args[1]["Item"]
        assert item["device_id"] == SC_TEST_ID
        assert item["timestamp_source"] == "cloud"
        assert "cloud_received_mt" in item

    def test_has_timestamp_mt_not_timestamp(self):
        """Sort key should be timestamp_mt (MT string), not timestamp (Unix ms)."""
        mock_tbl = MagicMock()
        with patch.object(sched, "table", mock_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID),              patch("charge_scheduler_lambda.time") as mock_time:
            mock_time.time.return_value = 1700000000.0
            sched.log_command_event("delay_window", "tou_peak", 85, True)
        item = mock_tbl.put_item.call_args[1]["Item"]
        assert "timestamp_mt" in item
        assert "timestamp" not in item
        # timestamp_mt should be a string like "YYYY-MM-DD HH:MM:SS.mmm"
        assert isinstance(item["timestamp_mt"], str)
        assert len(item["timestamp_mt"]) == 23  # "2023-11-14 10:13:20.000"

    def test_includes_wireless_device_id(self):
        """Event should include the original wireless_device_id."""
        mock_tbl = MagicMock()
        with patch.object(sched, "table", mock_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID),              patch("charge_scheduler_lambda.time") as mock_time:
            mock_time.time.return_value = 1700000000.0
            sched.log_command_event("allow", "off_peak", None, False)
        item = mock_tbl.put_item.call_args[1]["Item"]
        assert item["wireless_device_id"] == "test-device-id"

    def test_includes_ttl(self):
        """Event should include TTL for DynamoDB auto-expiry (90 days)."""
        mock_tbl = MagicMock()
        with patch.object(sched, "table", mock_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID),              patch("charge_scheduler_lambda.time") as mock_time:
            mock_time.time.return_value = 1700000000.0
            sched.log_command_event("allow", "off_peak", None, False)
        item = mock_tbl.put_item.call_args[1]["Item"]
        assert "ttl" in item
        # TTL should be ~90 days (7776000s) after the event
        expected_ttl = 1700000000 + 7776000
        assert item["ttl"] == expected_ttl

    def test_includes_event_fields(self):
        """Event should include command, reason, moer, tou fields."""
        mock_tbl = MagicMock()
        with patch.object(sched, "table", mock_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID),              patch("charge_scheduler_lambda.time") as mock_time:
            mock_time.time.return_value = 1700000000.0
            sched.log_command_event("delay_window", "tou_peak, moer>70", 85, True)
        item = mock_tbl.put_item.call_args[1]["Item"]
        assert item["event_type"] == "charge_scheduler_command"
        assert item["command"] == "delay_window"
        assert item["reason"] == "tou_peak, moer>70"
        assert item["moer_percent"] == 85
        assert item["tou_peak"] is True
        assert item["timestamp_source"] == "cloud"
        assert item["cloud_received_mt"] == item["timestamp_mt"]

    def test_moer_none_stored_as_na(self):
        """None moer_percent should be stored as 'N/A'."""
        mock_tbl = MagicMock()
        with patch.object(sched, "table", mock_tbl),              patch.object(sched, "_get_sc_id", return_value=SC_TEST_ID),              patch("charge_scheduler_lambda.time") as mock_time:
            mock_time.time.return_value = 1700000000.0
            sched.log_command_event("allow", "off_peak", None, False)
        item = mock_tbl.put_item.call_args[1]["Item"]
        assert item["moer_percent"] == "N/A"
