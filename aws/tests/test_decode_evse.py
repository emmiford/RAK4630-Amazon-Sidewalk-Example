"""Tests for decode_evse_lambda.py — EVSE payload decoding + OTA routing."""

import base64
import os
import struct
import sys
from unittest.mock import MagicMock, patch

import pytest

# Module-level mocking (must happen before import)
mock_sidewalk_utils = MagicMock()
mock_sidewalk_utils.get_device_id.return_value = "test-device-id"
sys.modules["sidewalk_utils"] = mock_sidewalk_utils

if "boto3" not in sys.modules:
    sys.modules["boto3"] = MagicMock()

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import decode_evse_lambda as decode  # noqa: E402


def encode_b64(raw_bytes):
    """Helper: encode raw bytes to base64 string."""
    return base64.b64encode(raw_bytes).decode()


# --- Raw EVSE payload decoding ---

class TestDecodeRawEvsePayload:
    def test_valid_state_a(self):
        """State A (0x01), 2980mV, 0mA, no thermostat."""
        raw = bytes([0xE5, 0x01, 0x01, 0xA4, 0x0B, 0x00, 0x00, 0x00])
        result = decode.decode_raw_evse_payload(raw)
        assert result is not None
        assert result["payload_type"] == "evse"
        assert result["j1772_state_code"] == 1
        assert result["j1772_state"] == "A"
        assert result["pilot_voltage_mv"] == 2980
        assert result["current_ma"] == 0

    def test_valid_state_c_with_current(self):
        """State C (0x03), 1489mV, 15000mA, heat+cool active."""
        voltage = 1489  # 0x05D1
        current = 15000  # 0x3A98
        raw = bytes([0xE5, 0x01, 0x03,
                     voltage & 0xFF, (voltage >> 8) & 0xFF,
                     current & 0xFF, (current >> 8) & 0xFF,
                     0x03])
        result = decode.decode_raw_evse_payload(raw)
        assert result["j1772_state_code"] == 3
        assert result["j1772_state"] == "C"
        assert result["pilot_voltage_mv"] == 1489
        assert result["current_ma"] == 15000
        assert result["thermostat_heat"] is True
        assert result["thermostat_cool"] is True

    def test_wrong_magic_returns_none(self):
        raw = bytes([0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00])
        assert decode.decode_raw_evse_payload(raw) is None

    def test_too_short_returns_none(self):
        raw = bytes([0xE5, 0x01, 0x01])
        assert decode.decode_raw_evse_payload(raw) is None

    def test_invalid_j1772_state_returns_none(self):
        raw = bytes([0xE5, 0x01, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00])
        assert decode.decode_raw_evse_payload(raw) is None

    def test_voltage_out_of_range_returns_none(self):
        """Pilot voltage > 15000 should fail sanity check."""
        voltage = 20000
        raw = bytes([0xE5, 0x01, 0x01,
                     voltage & 0xFF, (voltage >> 8) & 0xFF,
                     0x00, 0x00, 0x00])
        assert decode.decode_raw_evse_payload(raw) is None

    def test_thermostat_heat_only(self):
        raw = bytes([0xE5, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01])
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_heat"] is True
        assert result["thermostat_cool"] is False

    def test_thermostat_cool_only(self):
        raw = bytes([0xE5, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02])
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_heat"] is False
        assert result["thermostat_cool"] is True

    def test_all_j1772_states(self):
        """All valid J1772 states (0-6) should decode."""
        for state_code in range(7):
            raw = bytes([0xE5, 0x01, state_code, 0x00, 0x00, 0x00, 0x00, 0x00])
            result = decode.decode_raw_evse_payload(raw)
            assert result is not None, f"State {state_code} failed"
            assert result["j1772_state_code"] == state_code


# --- OTA uplink decoding ---

class TestDecodeOtaUplink:
    def test_ack_payload(self):
        raw = struct.pack("<BBbHH", 0x20, 0x80, 0, 5, 5)
        result = decode.decode_ota_uplink(raw)
        assert result["ota_type"] == "ack"
        assert result["status"] == 0
        assert result["next_chunk"] == 5
        assert result["chunks_received"] == 5

    def test_complete_payload(self):
        raw = struct.pack("<BBbI", 0x20, 0x81, 0, 0xDEADBEEF)
        result = decode.decode_ota_uplink(raw)
        assert result["ota_type"] == "complete"
        assert result["result"] == 0
        assert result["crc32_calc"] == "0xdeadbeef"

    def test_status_payload(self):
        raw = struct.pack("<BBbHHI", 0x20, 0x82, 1, 10, 20, 6)
        result = decode.decode_ota_uplink(raw)
        assert result["ota_type"] == "status"
        assert result["phase"] == 1
        assert result["chunks_received"] == 10
        assert result["total_chunks"] == 20
        assert result["app_version"] == 6

    def test_not_ota_returns_none(self):
        raw = bytes([0xE5, 0x01, 0x00, 0x00, 0x00])
        assert decode.decode_ota_uplink(raw) is None

    def test_unknown_subtype(self):
        raw = bytes([0x20, 0x99, 0x00, 0x00, 0x00])
        result = decode.decode_ota_uplink(raw)
        assert result["ota_type"] == "unknown"

    def test_ack_too_short(self):
        raw = bytes([0x20, 0x80, 0x00])
        result = decode.decode_ota_uplink(raw)
        assert result["ota_type"] == "unknown"


# --- Full decode_payload (base64 → decoded) ---

class TestDecodePayload:
    def test_raw_evse_via_b64(self):
        raw = bytes([0xE5, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00])
        result = decode.decode_payload(encode_b64(raw))
        assert result["payload_type"] == "evse"
        assert result["j1772_state_code"] == 3

    def test_ota_ack_via_b64(self):
        raw = struct.pack("<BBbHH", 0x20, 0x80, 0, 1, 1)
        result = decode.decode_payload(encode_b64(raw))
        assert result["payload_type"] == "ota"
        assert result["ota_type"] == "ack"

    def test_garbage_returns_unknown(self):
        raw = bytes([0xAB, 0xCD])
        result = decode.decode_payload(encode_b64(raw))
        assert result["payload_type"] == "unknown"

    def test_bad_base64_returns_error(self):
        result = decode.decode_payload("not-valid-base64!!!")
        assert result["payload_type"] in ("error", "unknown")


# --- Lambda handler integration ---

class TestLambdaHandler:
    @pytest.fixture(autouse=True)
    def mock_dynamodb(self):
        """Mock the DynamoDB table and TIME_SYNC auto-send."""
        with patch.object(decode, "table") as mock_table, \
             patch.object(decode, "maybe_send_time_sync"):
            mock_table.put_item = MagicMock()
            self.mock_table = mock_table
            yield

    def _make_event(self, raw_bytes):
        return {
            "WirelessDeviceId": "test-device",
            "PayloadData": encode_b64(raw_bytes),
            "WirelessMetadata": {
                "Sidewalk": {
                    "LinkType": "LoRa",
                    "Rssi": -80,
                    "Seq": 42,
                    "Timestamp": "2025-01-01T00:00:00Z",
                    "SidewalkId": "sid-123",
                }
            },
        }

    def test_evse_telemetry_stored(self):
        raw = bytes([0xE5, 0x01, 0x01, 0xA4, 0x0B, 0x00, 0x00, 0x00])
        result = decode.lambda_handler(self._make_event(raw), None)
        assert result["statusCode"] == 200
        self.mock_table.put_item.assert_called_once()
        item = self.mock_table.put_item.call_args[1]["Item"]
        assert item["event_type"] == "evse_telemetry"

    def test_ota_ack_forwarded(self):
        raw = struct.pack("<BBbHH", 0x20, 0x80, 0, 1, 1)
        with patch.object(decode, "lambda_client") as mock_lambda:
            result = decode.lambda_handler(self._make_event(raw), None)
        assert result["statusCode"] == 200
        mock_lambda.invoke.assert_called_once()
        call_kwargs = mock_lambda.invoke.call_args[1]
        assert call_kwargs["InvocationType"] == "Event"


# --- Fault flag decoding ---

class TestFaultFlags:
    def test_no_faults_all_false(self):
        """Byte 7 = 0x00 → no fault flags."""
        raw = bytes([0xE5, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00])
        result = decode.decode_raw_evse_payload(raw)
        assert result["fault_sensor"] is False
        assert result["fault_clamp_mismatch"] is False
        assert result["fault_interlock"] is False
        assert result["fault_selftest_fail"] is False

    def test_selftest_fail_flag(self):
        """Byte 7 = 0x80 → selftest fail, no thermostat."""
        raw = bytes([0xE5, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80])
        result = decode.decode_raw_evse_payload(raw)
        assert result["fault_selftest_fail"] is True
        assert result["fault_sensor"] is False
        assert result["thermostat_heat"] is False
        assert result["thermostat_cool"] is False
        assert result["thermostat_bits"] == 0

    def test_faults_coexist_with_thermostat(self):
        """Byte 7 = 0x93 → heat + cool + sensor fault + selftest fail."""
        raw = bytes([0xE5, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x93])
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_heat"] is True
        assert result["thermostat_cool"] is True
        assert result["thermostat_bits"] == 0x03
        assert result["fault_sensor"] is True
        assert result["fault_selftest_fail"] is True
        assert result["fault_clamp_mismatch"] is False
        assert result["fault_interlock"] is False

    def test_all_fault_flags_set(self):
        """Byte 7 = 0xF0 → all four fault flags."""
        raw = bytes([0xE5, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF0])
        result = decode.decode_raw_evse_payload(raw)
        assert result["fault_sensor"] is True
        assert result["fault_clamp_mismatch"] is True
        assert result["fault_interlock"] is True
        assert result["fault_selftest_fail"] is True
        assert result["thermostat_heat"] is False
        assert result["thermostat_cool"] is False
