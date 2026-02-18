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
        # 9 PM MT today as SideCharge epoch
        expected_9pm = now.replace(hour=21, minute=0, second=0, microsecond=0)
        expected_sc = int(expected_9pm.timestamp()) - sched.SIDECHARGE_EPOCH_OFFSET
        assert end_sc == expected_sc

    def test_peak_end_at_5pm_still_9pm(self):
        now = datetime(2026, 2, 16, 17, 0, tzinfo=MT)  # Monday 5 PM
        end_sc = sched.get_tou_peak_end_sc(now)
        expected_9pm = now.replace(hour=21, minute=0, second=0, microsecond=0)
        expected_sc = int(expected_9pm.timestamp()) - sched.SIDECHARGE_EPOCH_OFFSET
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
        """SideCharge epoch values can be large 32-bit numbers."""
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
        expected_end_sc = int(expected_9pm.timestamp()) - sched.SIDECHARGE_EPOCH_OFFSET
        assert end_sc == expected_end_sc


# --- Heartbeat re-send logic ---

class TestHeartbeat:
    def test_recent_same_window_skips(self):
        """If same window was sent <30 min ago, skip."""
        now = datetime(2026, 2, 16, 18, 0, tzinfo=MT)
        now_unix = int(now.timestamp())
        now_sc = now_unix - sched.SIDECHARGE_EPOCH_OFFSET
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
