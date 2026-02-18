"""Tests for TIME_SYNC downlink in decode_evse_lambda.py."""

import base64
import os
import sys
import time
from unittest.mock import MagicMock, patch

import pytest

# Module-level mocking (must happen before import)
if "sidewalk_utils" not in sys.modules:
    mock_sidewalk_utils = MagicMock()
    mock_sidewalk_utils.get_device_id.return_value = "test-device-id"
    mock_sidewalk_utils.send_sidewalk_msg = MagicMock()
    sys.modules["sidewalk_utils"] = mock_sidewalk_utils

if "boto3" not in sys.modules:
    sys.modules["boto3"] = MagicMock()

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import decode_evse_lambda as decode  # noqa: E402


# --- TIME_SYNC payload builder ---

class TestBuildTimeSyncBytes:
    def test_payload_length(self):
        payload = decode._build_time_sync_bytes(1000, 500)
        assert len(payload) == 9

    def test_command_type(self):
        payload = decode._build_time_sync_bytes(0, 0)
        assert payload[0] == 0x30

    def test_epoch_encoding_le(self):
        """SideCharge epoch is 4 bytes little-endian at offset 1."""
        epoch = 0x12345678
        payload = decode._build_time_sync_bytes(epoch, 0)
        assert payload[1] == 0x78
        assert payload[2] == 0x56
        assert payload[3] == 0x34
        assert payload[4] == 0x12

    def test_watermark_encoding_le(self):
        """ACK watermark is 4 bytes little-endian at offset 5."""
        watermark = 0xAABBCCDD
        payload = decode._build_time_sync_bytes(0, watermark)
        assert payload[5] == 0xDD
        assert payload[6] == 0xCC
        assert payload[7] == 0xBB
        assert payload[8] == 0xAA

    def test_round_trip_values(self):
        epoch = 86400  # 1 day
        wm = 86000
        payload = decode._build_time_sync_bytes(epoch, wm)

        # Decode back
        parsed_epoch = int.from_bytes(payload[1:5], "little")
        parsed_wm = int.from_bytes(payload[5:9], "little")
        assert parsed_epoch == epoch
        assert parsed_wm == wm


# --- SideCharge epoch math ---

class TestSideChargeEpoch:
    def test_epoch_offset_value(self):
        """SIDECHARGE_EPOCH_OFFSET should be 2026-01-01T00:00:00Z."""
        assert decode.SIDECHARGE_EPOCH_OFFSET == 1767225600

    def test_epoch_conversion(self):
        """Unix timestamp → SideCharge epoch."""
        # 2026-01-02T00:00:00Z = 1767225600 + 86400
        unix_ts = 1767225600 + 86400
        sc_epoch = unix_ts - decode.SIDECHARGE_EPOCH_OFFSET
        assert sc_epoch == 86400


# --- maybe_send_time_sync ---

class TestMaybeSendTimeSync:
    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_sends_when_no_sentinel(self, mock_put, mock_get, mock_send):
        """Should send TIME_SYNC when no sentinel exists."""
        mock_get.return_value = {}  # no Item key
        decode.maybe_send_time_sync("dev-001")
        mock_send.assert_called_once()
        payload = mock_send.call_args[0][0]
        assert len(payload) == 9
        assert payload[0] == 0x30

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_skips_when_recently_synced(self, mock_put, mock_get, mock_send):
        """Should NOT send TIME_SYNC if sentinel < 24h old."""
        mock_get.return_value = {
            "Item": {"last_sync_unix": int(time.time()) - 3600}  # 1 hour ago
        }
        decode.maybe_send_time_sync("dev-001")
        mock_send.assert_not_called()

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_sends_when_sentinel_expired(self, mock_put, mock_get, mock_send):
        """Should send TIME_SYNC if sentinel > 24h old."""
        mock_get.return_value = {
            "Item": {"last_sync_unix": int(time.time()) - 100000}  # >24h
        }
        decode.maybe_send_time_sync("dev-001")
        mock_send.assert_called_once()

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_updates_sentinel_after_send(self, mock_put, mock_get, mock_send):
        """Should write sentinel with current time after sending."""
        mock_get.return_value = {}
        decode.maybe_send_time_sync("dev-001")

        # put_item should be called with sentinel
        assert mock_put.called
        sentinel = mock_put.call_args[1]["Item"]
        assert sentinel["device_id"] == "dev-001"
        assert sentinel["timestamp"] == -2
        assert sentinel["event_type"] == "time_sync_state"
        assert "last_sync_unix" in sentinel
        assert "last_sync_epoch" in sentinel

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_payload_epoch_is_current(self, mock_put, mock_get, mock_send):
        """Payload epoch should be approximately now - SIDECHARGE_EPOCH_OFFSET."""
        mock_get.return_value = {}
        before = int(time.time()) - decode.SIDECHARGE_EPOCH_OFFSET
        decode.maybe_send_time_sync("dev-001")
        after = int(time.time()) - decode.SIDECHARGE_EPOCH_OFFSET

        payload = mock_send.call_args[0][0]
        epoch = int.from_bytes(payload[1:5], "little")
        assert before <= epoch <= after

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item", side_effect=Exception("DB error"))
    def test_sentinel_read_error_still_sends(self, mock_get, mock_send):
        """On sentinel read error, should still attempt to send."""
        decode.maybe_send_time_sync("dev-001")
        # After an exception in the try block, the function falls through
        # and still attempts to send
        mock_send.assert_called_once()

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_forces_sync_when_device_timestamp_zero(self, mock_put, mock_get, mock_send):
        """Should force TIME_SYNC when device reports timestamp=0 (unsynced)."""
        mock_get.return_value = {
            "Item": {"last_sync_unix": int(time.time()) - 60}  # 1 min ago
        }
        decode.maybe_send_time_sync("dev-001", device_timestamp=0)
        mock_send.assert_called_once()
        assert mock_send.call_args[0][0][0] == 0x30

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_respects_sentinel_when_device_has_timestamp(self, mock_put, mock_get, mock_send):
        """Should respect sentinel TTL when device has a valid timestamp."""
        mock_get.return_value = {
            "Item": {"last_sync_unix": int(time.time()) - 60}  # 1 min ago
        }
        decode.maybe_send_time_sync("dev-001", device_timestamp=4157000)
        mock_send.assert_not_called()

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "get_item")
    @patch.object(decode.table, "put_item")
    def test_forces_sync_when_device_timestamp_none(self, mock_put, mock_get, mock_send):
        """Should use sentinel logic when device_timestamp is None (v0x06)."""
        mock_get.return_value = {
            "Item": {"last_sync_unix": int(time.time()) - 60}  # 1 min ago
        }
        decode.maybe_send_time_sync("dev-001", device_timestamp=None)
        mock_send.assert_not_called()


# --- Integration: TIME_SYNC triggered on EVSE uplink ---

class TestTimeSyncIntegration:
    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "put_item")
    @patch.object(decode.table, "get_item")
    def test_time_sync_sent_on_evse_uplink(self, mock_get, mock_put, mock_send):
        """EVSE telemetry should trigger maybe_send_time_sync."""
        mock_get.return_value = {}  # no sentinel

        raw = bytes([0xE5, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00])
        event = {
            "WirelessDeviceId": "dev-001",
            "PayloadData": base64.b64encode(raw).decode(),
            "WirelessMetadata": {"Sidewalk": {}},
        }
        decode.lambda_handler(event, None)

        # Find the TIME_SYNC call (0x30 prefix)
        assert mock_send.called
        for call in mock_send.call_args_list:
            payload = call[0][0]
            if payload[0] == 0x30:
                assert len(payload) == 9
                return
        pytest.fail("No TIME_SYNC payload found in send_sidewalk_msg calls")

    @patch("decode_evse_lambda.send_sidewalk_msg")
    @patch.object(decode.table, "put_item")
    @patch.object(decode.table, "get_item")
    def test_no_time_sync_on_ota_uplink(self, mock_get, mock_put, mock_send):
        """OTA uplinks should NOT trigger TIME_SYNC."""
        mock_get.return_value = {}

        # OTA ACK payload
        raw = bytes([0x20, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00])
        event = {
            "WirelessDeviceId": "dev-001",
            "PayloadData": base64.b64encode(raw).decode(),
            "WirelessMetadata": {"Sidewalk": {}},
        }
        decode.lambda_handler(event, None)

        # No TIME_SYNC should be sent (0x30 prefix)
        for call in mock_send.call_args_list:
            payload = call[0][0]
            if isinstance(payload, (bytes, bytearray)) and len(payload) > 0:
                assert payload[0] != 0x30, "TIME_SYNC should not be sent on OTA uplink"
