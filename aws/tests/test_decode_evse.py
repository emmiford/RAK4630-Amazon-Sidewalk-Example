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

    def test_ttl_attribute_set_for_90_day_retention(self):
        """DynamoDB items must have a ttl attribute for automatic expiration."""
        raw = bytes([0xE5, 0x01, 0x01, 0xA4, 0x0B, 0x00, 0x00, 0x00])
        result = decode.lambda_handler(self._make_event(raw), None)
        assert result["statusCode"] == 200
        item = self.mock_table.put_item.call_args[1]["Item"]
        assert "ttl" in item
        # TTL should be ~90 days (7776000s) after the event timestamp
        expected_ttl = int(item["timestamp"] / 1000) + 7776000
        assert item["ttl"] == expected_ttl

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


# --- v0x07 payload decoding (12 bytes with timestamp + control flags) ---

class TestDecodeV07Payload:
    def _make_v07(self, j1772=1, voltage=0, current=0, flags=0, timestamp=0):
        """Helper: build a 12-byte v0x07 payload."""
        return bytes([
            0xE5, 0x07, j1772,
            voltage & 0xFF, (voltage >> 8) & 0xFF,
            current & 0xFF, (current >> 8) & 0xFF,
            flags,
            timestamp & 0xFF, (timestamp >> 8) & 0xFF,
            (timestamp >> 16) & 0xFF, (timestamp >> 24) & 0xFF,
        ])

    def test_v07_basic_decode(self):
        """12-byte v0x07 payload decodes with timestamp."""
        raw = self._make_v07(j1772=3, voltage=1489, current=15000,
                             flags=0x07, timestamp=86400)
        result = decode.decode_raw_evse_payload(raw)
        assert result is not None
        assert result["version"] == 0x07
        assert result["j1772_state_code"] == 3
        assert result["pilot_voltage_mv"] == 1489
        assert result["current_ma"] == 15000
        assert result["device_timestamp_epoch"] == 86400

    def test_v07_timestamp_to_unix(self):
        """device_timestamp_unix = epoch + SIDECHARGE_EPOCH_OFFSET."""
        raw = self._make_v07(timestamp=86400)
        result = decode.decode_raw_evse_payload(raw)
        assert result["device_timestamp_unix"] == 86400 + decode.SIDECHARGE_EPOCH_OFFSET

    def test_v07_timestamp_zero_means_not_synced(self):
        """Timestamp=0 means device not yet synced."""
        raw = self._make_v07(timestamp=0)
        result = decode.decode_raw_evse_payload(raw)
        assert result["device_timestamp_epoch"] == 0
        assert result["device_timestamp_unix"] is None

    def test_v07_charge_allowed_flag(self):
        """Bit 2 of flags byte = charge_allowed."""
        raw = self._make_v07(flags=0x04)
        result = decode.decode_raw_evse_payload(raw)
        assert result["charge_allowed"] is True
        assert result["charge_now"] is False

    def test_v07_charge_now_flag(self):
        """Bit 3 of flags byte = charge_now."""
        raw = self._make_v07(flags=0x08)
        result = decode.decode_raw_evse_payload(raw)
        assert result["charge_now"] is True
        assert result["charge_allowed"] is False

    def test_v07_all_flags_coexist(self):
        """All flag bits: heat + cool + charge_allowed + charge_now + all faults."""
        raw = self._make_v07(flags=0xFF)
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_heat"] is True
        assert result["thermostat_cool"] is True
        assert result["charge_allowed"] is True
        assert result["charge_now"] is True
        assert result["fault_sensor"] is True
        assert result["fault_clamp_mismatch"] is True
        assert result["fault_interlock"] is True
        assert result["fault_selftest_fail"] is True

    def test_v07_thermostat_bits_only_bits_0_1(self):
        """thermostat_bits should only contain bits 0-1, not control flags."""
        raw = self._make_v07(flags=0x0F)  # heat + cool + charge_allowed + charge_now
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_bits"] == 0x03  # Only heat + cool

    def test_v07_large_timestamp(self):
        """Max 32-bit timestamp (year ~2162)."""
        raw = self._make_v07(timestamp=0xFFFFFFFF)
        result = decode.decode_raw_evse_payload(raw)
        assert result["device_timestamp_epoch"] == 0xFFFFFFFF

    def test_v06_backward_compat(self):
        """8-byte v0x06 payload still decodes (no timestamp fields)."""
        raw = bytes([0xE5, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00])
        result = decode.decode_raw_evse_payload(raw)
        assert result is not None
        assert result["version"] == 0x06
        assert "device_timestamp_epoch" not in result

    def test_v07_via_b64(self):
        """v0x07 payload through full base64 decode pipeline."""
        raw = self._make_v07(j1772=2, voltage=2234, timestamp=1000)
        result = decode.decode_payload(encode_b64(raw))
        assert result["payload_type"] == "evse"
        assert result["version"] == 0x07
        assert result["device_timestamp_epoch"] == 1000


# --- v0x08 payload decoding (12 bytes, no heat flag) ---

class TestDecodeV08Payload:
    def _make_v08(self, j1772=1, voltage=0, current=0, flags=0, timestamp=0):
        """Helper: build a 12-byte v0x08 payload."""
        return bytes([
            0xE5, 0x08, j1772,
            voltage & 0xFF, (voltage >> 8) & 0xFF,
            current & 0xFF, (current >> 8) & 0xFF,
            flags,
            timestamp & 0xFF, (timestamp >> 8) & 0xFF,
            (timestamp >> 16) & 0xFF, (timestamp >> 24) & 0xFF,
        ])

    def test_v08_basic_decode(self):
        """12-byte v0x08 payload decodes correctly."""
        raw = self._make_v08(j1772=3, voltage=1489, current=15000,
                             flags=0x06, timestamp=86400)
        result = decode.decode_raw_evse_payload(raw)
        assert result is not None
        assert result["version"] == 0x08
        assert result["j1772_state_code"] == 3
        assert result["pilot_voltage_mv"] == 1489
        assert result["current_ma"] == 15000
        assert result["device_timestamp_epoch"] == 86400

    def test_v08_no_heat_flag(self):
        """v0x08 does not include thermostat_heat in decoded output."""
        raw = self._make_v08(flags=0x03)  # bit 0 set, but reserved in v0x08
        result = decode.decode_raw_evse_payload(raw)
        assert "thermostat_heat" not in result
        assert result["thermostat_cool"] is True

    def test_v08_thermostat_bits_only_bit1(self):
        """v0x08 thermostat_bits should only include bit 1 (cool)."""
        raw = self._make_v08(flags=0x03)  # bits 0+1 set
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_bits"] == 0x02  # Only cool bit

    def test_v08_cool_flag(self):
        """v0x08 cool flag decodes from bit 1."""
        raw = self._make_v08(flags=0x02)  # cool only
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_cool"] is True
        assert result["thermostat_bits"] == 0x02

    def test_v08_no_cool(self):
        """v0x08 with no cool flag."""
        raw = self._make_v08(flags=0x00)
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_cool"] is False
        assert result["thermostat_bits"] == 0x00

    def test_v08_charge_allowed(self):
        """v0x08 charge_allowed still works."""
        raw = self._make_v08(flags=0x04)
        result = decode.decode_raw_evse_payload(raw)
        assert result["charge_allowed"] is True

    def test_v08_all_flags_except_heat(self):
        """v0x08 all flags: cool + charge_allowed + charge_now + faults."""
        raw = self._make_v08(flags=0xFE)  # all bits except bit 0
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_cool"] is True
        assert result["charge_allowed"] is True
        assert result["charge_now"] is True
        assert result["fault_sensor"] is True
        assert result["fault_clamp_mismatch"] is True
        assert result["fault_interlock"] is True
        assert result["fault_selftest_fail"] is True
        assert "thermostat_heat" not in result

    def test_v08_timestamp(self):
        """v0x08 timestamp works same as v0x07."""
        raw = self._make_v08(timestamp=86400)
        result = decode.decode_raw_evse_payload(raw)
        assert result["device_timestamp_epoch"] == 86400
        assert result["device_timestamp_unix"] == 86400 + decode.SIDECHARGE_EPOCH_OFFSET

    def test_v08_via_b64(self):
        """v0x08 payload through full base64 decode pipeline."""
        raw = self._make_v08(j1772=2, voltage=2234, timestamp=1000)
        result = decode.decode_payload(encode_b64(raw))
        assert result["payload_type"] == "evse"
        assert result["version"] == 0x08
        assert "thermostat_heat" not in result

    def test_v07_backward_compat_still_has_heat(self):
        """v0x07 payloads still include thermostat_heat for backward compat."""
        raw = bytes([
            0xE5, 0x07, 0x01,
            0x00, 0x00, 0x00, 0x00,
            0x01,  # heat flag set
            0x00, 0x00, 0x00, 0x00,
        ])
        result = decode.decode_raw_evse_payload(raw)
        assert result["thermostat_heat"] is True
        assert result["thermostat_bits"] == 0x01


# --- Diagnostics payload decoding (0xE6) ---

class TestDecodeDiagPayload:
    @staticmethod
    def _make_diag(diag_ver=1, app_ver=3, uptime=120, boot_count=0,
                   error_code=0, state_flags=0x43, pending=5):
        """Build a 14-byte diagnostics payload."""
        return bytes([
            0xE6, diag_ver,
            app_ver & 0xFF, (app_ver >> 8) & 0xFF,
            uptime & 0xFF, (uptime >> 8) & 0xFF,
            (uptime >> 16) & 0xFF, (uptime >> 24) & 0xFF,
            boot_count & 0xFF, (boot_count >> 8) & 0xFF,
            error_code,
            state_flags,
            pending,
            0x00,
        ])

    def test_valid_diag_payload(self):
        """Basic diagnostics decode with all fields."""
        raw = self._make_diag(app_ver=3, uptime=120, state_flags=0x43, pending=5)
        result = decode.decode_diag_payload(raw)
        assert result is not None
        assert result["payload_type"] == "diagnostics"
        assert result["diag_version"] == 1
        assert result["app_version"] == 3
        assert result["uptime_seconds"] == 120
        assert result["boot_count"] == 0
        assert result["last_error_code"] == 0
        assert result["last_error_name"] == "none"
        assert result["event_buffer_pending"] == 5

    def test_state_flags_decode(self):
        """State flags 0x43 = SIDEWALK_READY | CHARGE_ALLOWED | TIME_SYNCED."""
        raw = self._make_diag(state_flags=0x43)
        result = decode.decode_diag_payload(raw)
        assert result["sidewalk_ready"] is True
        assert result["charge_allowed"] is True
        assert result["charge_now"] is False
        assert result["interlock_active"] is False
        assert result["selftest_pass"] is False
        assert result["ota_in_progress"] is False
        assert result["time_synced"] is True

    def test_all_state_flags_set(self):
        """All state flags set (0x7F)."""
        raw = self._make_diag(state_flags=0x7F)
        result = decode.decode_diag_payload(raw)
        assert result["sidewalk_ready"] is True
        assert result["charge_allowed"] is True
        assert result["charge_now"] is True
        assert result["interlock_active"] is True
        assert result["selftest_pass"] is True
        assert result["ota_in_progress"] is True
        assert result["time_synced"] is True

    def test_error_code_sensor(self):
        """Error code 1 = sensor fault."""
        raw = self._make_diag(error_code=1)
        result = decode.decode_diag_payload(raw)
        assert result["last_error_code"] == 1
        assert result["last_error_name"] == "sensor"

    def test_error_code_selftest(self):
        """Error code 4 = selftest failure."""
        raw = self._make_diag(error_code=4)
        result = decode.decode_diag_payload(raw)
        assert result["last_error_code"] == 4
        assert result["last_error_name"] == "selftest"

    def test_error_code_unknown(self):
        """Unknown error code falls back to 'unknown_N'."""
        raw = self._make_diag(error_code=99)
        result = decode.decode_diag_payload(raw)
        assert result["last_error_name"] == "unknown_99"

    def test_large_uptime(self):
        """Large uptime value (27 hours = 97200s)."""
        raw = self._make_diag(uptime=97200)
        result = decode.decode_diag_payload(raw)
        assert result["uptime_seconds"] == 97200

    def test_too_short_returns_none(self):
        """Payload shorter than 14 bytes returns None."""
        raw = bytes([0xE6, 0x01, 0x03, 0x00])
        result = decode.decode_diag_payload(raw)
        assert result is None

    def test_wrong_magic_returns_none(self):
        """Non-0xE6 magic byte returns None."""
        raw = bytes([0xE5]) + bytes(13)
        result = decode.decode_diag_payload(raw)
        assert result is None

    def test_diag_via_b64_pipeline(self):
        """Diagnostics payload through full base64 decode pipeline."""
        raw = self._make_diag(app_ver=5, uptime=1000, state_flags=0x13, pending=2)
        result = decode.decode_payload(encode_b64(raw))
        assert result["payload_type"] == "diagnostics"
        assert result["app_version"] == 5
        assert result["uptime_seconds"] == 1000

    def test_handler_stores_as_device_diagnostics(self):
        """Lambda handler stores diagnostics as event_type='device_diagnostics'."""
        raw = self._make_diag()
        event = {
            "WirelessDeviceId": "test-device-123",
            "PayloadData": encode_b64(raw),
            "WirelessMetadata": {
                "Sidewalk": {
                    "LinkType": "LORA",
                    "Rssi": -85,
                    "Seq": 42,
                    "Timestamp": "2026-02-17T12:00:00Z",
                    "SidewalkId": "sid-001",
                }
            },
        }

        with patch.object(decode.table, "put_item") as mock_put, \
             patch.object(decode, "maybe_send_time_sync"), \
             patch("device_registry.get_or_create_device"), \
             patch("device_registry.update_last_seen"):
            decode.lambda_handler(event, None)

        mock_put.assert_called_once()
        item = mock_put.call_args[1]["Item"]
        assert item["event_type"] == "device_diagnostics"
        assert "diagnostics" in item["data"]
        assert item["data"]["diagnostics"]["app_version"] == 3
