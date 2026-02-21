"""Tests for the health digest Lambda."""

import os
import sys
import time
from unittest.mock import MagicMock, patch

import pytest

# Ensure aws/ is on the path and boto3 is mocked
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
if "boto3" not in sys.modules:
    sys.modules["boto3"] = MagicMock()

import health_digest_lambda as hdl


# ================================================================
# Fixtures
# ================================================================

@pytest.fixture(autouse=True)
def reset_module():
    """Reset module-level state between tests."""
    hdl.HEARTBEAT_INTERVAL_S = 900
    hdl.SNS_TOPIC_ARN = "arn:aws:sns:us-east-1:123456:test-topic"
    hdl.AUTO_DIAG_ENABLED = False
    hdl.LATEST_APP_VERSION = 0


def make_device(device_id, wireless_id="wid-001", last_seen=None,
                app_version=3, status="active"):
    """Create a device registry record."""
    if last_seen is None:
        last_seen = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    return {
        "device_id": device_id,
        "wireless_device_id": wireless_id,
        "last_seen": last_seen,
        "app_version": app_version,
        "status": status,
    }


def make_event_item(wireless_id, timestamp_ms, faults=None):
    """Create a DynamoDB event item with optional fault flags."""
    evse_data = {
        "pilot_state": "C",
        "current_draw_ma": 5000,
    }
    if faults:
        for f in faults:
            evse_data[f] = True
    return {
        "device_id": wireless_id,
        "timestamp": timestamp_ms,
        "event_type": "evse_telemetry",
        "data": {"evse": evse_data},
    }


def make_diag_event(wireless_id, timestamp_ms, app_version=3, uptime=120,
                    boot_count=0, error_name="none", pending=0):
    """Create a DynamoDB device_diagnostics event item."""
    return {
        "device_id": wireless_id,
        "timestamp": timestamp_ms,
        "event_type": "device_diagnostics",
        "data": {
            "diagnostics": {
                "payload_type": "diagnostics",
                "diag_version": 1,
                "app_version": app_version,
                "uptime_seconds": uptime,
                "boot_count": boot_count,
                "last_error_name": error_name,
                "event_buffer_pending": pending,
            }
        },
    }


# ================================================================
# check_device_health
# ================================================================

class TestCheckDeviceHealth:
    def test_online_device(self):
        """Device seen recently is online."""
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60)
        )
        device = make_device("SC-AAA", last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, now)

        assert result["online"] is True
        assert result["device_id"] == "SC-AAA"
        assert result["seconds_since_seen"] is not None
        assert result["seconds_since_seen"] < 900 * 2

    def test_offline_device(self):
        """Device not seen for 2x heartbeat is offline."""
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 3600)
        )
        device = make_device("SC-BBB", last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, now)

        assert result["online"] is False

    def test_device_at_threshold_boundary(self):
        """Device seen exactly at 2x heartbeat is still online."""
        now = time.time()
        threshold = hdl.HEARTBEAT_INTERVAL_S * hdl.OFFLINE_THRESHOLD_MULTIPLIER
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - threshold)
        )
        device = make_device("SC-CCC", last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, now)

        assert result["online"] is True

    def test_device_just_past_threshold(self):
        """Device seen just past 2x heartbeat is offline."""
        now = time.time()
        threshold = hdl.HEARTBEAT_INTERVAL_S * hdl.OFFLINE_THRESHOLD_MULTIPLIER
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - threshold - 1)
        )
        device = make_device("SC-DDD", last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, now)

        assert result["online"] is False

    def test_device_with_faults(self):
        """Fault flags from recent events are included."""
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60)
        )
        device = make_device("SC-EEE", last_seen=last_seen)
        faults = ["fault_sensor", "fault_interlock"]

        with patch.object(hdl, "get_recent_faults", return_value=faults):
            result = hdl.check_device_health(device, now)

        assert result["online"] is True
        assert result["recent_faults"] == faults

    def test_device_never_seen(self):
        """Device with empty last_seen is offline."""
        device = make_device("SC-FFF", last_seen="")

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, time.time())

        assert result["online"] is False
        assert result["seconds_since_seen"] is None

    def test_includes_wireless_device_id(self):
        """Health result includes wireless_device_id for targeted sends."""
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60)
        )
        device = make_device("SC-GGG", wireless_id="wid-777",
                             last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, now)

        assert result["wireless_device_id"] == "wid-777"

    def test_unhealthy_reasons_offline(self):
        """Offline device has 'offline' in unhealthy_reasons."""
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 3600)
        )
        device = make_device("SC-HHH", last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, now)

        assert "offline" in result["unhealthy_reasons"]

    def test_unhealthy_reasons_faults(self):
        """Faulted device has 'faults' in unhealthy_reasons."""
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60)
        )
        device = make_device("SC-III", last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults",
                          return_value=["fault_sensor"]):
            result = hdl.check_device_health(device, now)

        assert "faults" in result["unhealthy_reasons"]
        assert "offline" not in result["unhealthy_reasons"]

    def test_unhealthy_reasons_healthy(self):
        """Healthy online device has empty unhealthy_reasons."""
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60)
        )
        device = make_device("SC-JJJ", last_seen=last_seen)

        with patch.object(hdl, "get_recent_faults", return_value=[]):
            result = hdl.check_device_health(device, now)

        assert result["unhealthy_reasons"] == []


# ================================================================
# identify_unhealthy_reasons
# ================================================================

class TestIdentifyUnhealthyReasons:
    def test_healthy(self):
        """Online device with no faults and current firmware is healthy."""
        reasons = hdl.identify_unhealthy_reasons(
            online=True, recent_faults=[], app_version=3
        )
        assert reasons == []

    def test_offline(self):
        """Offline device is unhealthy."""
        reasons = hdl.identify_unhealthy_reasons(
            online=False, recent_faults=[], app_version=3
        )
        assert reasons == ["offline"]

    def test_faults(self):
        """Faulted device is unhealthy."""
        reasons = hdl.identify_unhealthy_reasons(
            online=True, recent_faults=["fault_sensor"], app_version=3
        )
        assert reasons == ["faults"]

    def test_stale_firmware(self):
        """Device with old firmware is unhealthy when LATEST_APP_VERSION set."""
        hdl.LATEST_APP_VERSION = 5
        reasons = hdl.identify_unhealthy_reasons(
            online=True, recent_faults=[], app_version=3
        )
        assert reasons == ["stale_firmware"]

    def test_stale_firmware_skipped_when_zero(self):
        """Stale firmware check skipped when LATEST_APP_VERSION is 0."""
        hdl.LATEST_APP_VERSION = 0
        reasons = hdl.identify_unhealthy_reasons(
            online=True, recent_faults=[], app_version=1
        )
        assert reasons == []

    def test_current_firmware_not_flagged(self):
        """Device at latest version is not flagged."""
        hdl.LATEST_APP_VERSION = 3
        reasons = hdl.identify_unhealthy_reasons(
            online=True, recent_faults=[], app_version=3
        )
        assert reasons == []

    def test_multiple_reasons(self):
        """Device can have multiple unhealthy reasons."""
        hdl.LATEST_APP_VERSION = 5
        reasons = hdl.identify_unhealthy_reasons(
            online=False, recent_faults=["fault_sensor"], app_version=2
        )
        assert reasons == ["offline", "faults", "stale_firmware"]


# ================================================================
# send_diagnostic_requests
# ================================================================

class TestSendDiagnosticRequests:
    def test_sends_to_unhealthy_only(self):
        """Only unhealthy devices receive diagnostic requests."""
        health_list = [
            {"device_id": "SC-001", "wireless_device_id": "wid-001",
             "unhealthy_reasons": ["offline"]},
            {"device_id": "SC-002", "wireless_device_id": "wid-002",
             "unhealthy_reasons": []},
            {"device_id": "SC-003", "wireless_device_id": "wid-003",
             "unhealthy_reasons": ["faults"]},
        ]

        with patch.object(hdl, "send_sidewalk_msg") as mock_send:
            queried = hdl.send_diagnostic_requests(health_list)

        assert queried == ["SC-001", "SC-003"]
        assert mock_send.call_count == 2
        # Verify 0x40 byte sent to correct device IDs
        for call in mock_send.call_args_list:
            assert call[0][0] == bytes([0x40])
            assert "wireless_device_id" in call[1]

    def test_skips_device_without_wireless_id(self):
        """Device without wireless_device_id is skipped."""
        health_list = [
            {"device_id": "SC-001", "wireless_device_id": "",
             "unhealthy_reasons": ["offline"]},
        ]

        with patch.object(hdl, "send_sidewalk_msg") as mock_send:
            queried = hdl.send_diagnostic_requests(health_list)

        assert queried == []
        mock_send.assert_not_called()

    def test_send_failure_continues(self):
        """Failure to send to one device doesn't block others."""
        health_list = [
            {"device_id": "SC-001", "wireless_device_id": "wid-001",
             "unhealthy_reasons": ["offline"]},
            {"device_id": "SC-002", "wireless_device_id": "wid-002",
             "unhealthy_reasons": ["faults"]},
        ]

        with patch.object(hdl, "send_sidewalk_msg") as mock_send:
            mock_send.side_effect = [Exception("Timeout"), None]
            queried = hdl.send_diagnostic_requests(health_list)

        # First failed, second succeeded
        assert queried == ["SC-002"]

    def test_no_unhealthy_devices(self):
        """No requests sent when all devices are healthy."""
        health_list = [
            {"device_id": "SC-001", "wireless_device_id": "wid-001",
             "unhealthy_reasons": []},
        ]

        with patch.object(hdl, "send_sidewalk_msg") as mock_send:
            queried = hdl.send_diagnostic_requests(health_list)

        assert queried == []
        mock_send.assert_not_called()

    def test_sends_correct_wireless_device_id(self):
        """Verify the wireless_device_id kwarg matches the device."""
        health_list = [
            {"device_id": "SC-001", "wireless_device_id": "wid-xyz",
             "unhealthy_reasons": ["offline"]},
        ]

        with patch.object(hdl, "send_sidewalk_msg") as mock_send:
            hdl.send_diagnostic_requests(health_list)

        mock_send.assert_called_once_with(
            bytes([0x40]), wireless_device_id="wid-xyz"
        )


# ================================================================
# get_recent_diagnostics
# ================================================================

class TestGetRecentDiagnostics:
    def test_returns_most_recent(self):
        """Returns the most recent diagnostics response."""
        diag_item = make_diag_event("wid-001", 1000000, app_version=5,
                                    uptime=3600, boot_count=2)

        with patch.object(hdl.events_table, "query",
                          return_value={"Items": [diag_item]}):
            result = hdl.get_recent_diagnostics("wid-001", time.time())

        assert result is not None
        assert result["app_version"] == 5
        assert result["uptime_seconds"] == 3600
        assert result["boot_count"] == 2

    def test_no_diagnostics(self):
        """Returns None when no diagnostics events exist."""
        with patch.object(hdl.events_table, "query",
                          return_value={"Items": []}):
            result = hdl.get_recent_diagnostics("wid-001", time.time())

        assert result is None

    def test_query_failure_returns_none(self):
        """DynamoDB failure returns None."""
        with patch.object(hdl.events_table, "query",
                          side_effect=Exception("Timeout")):
            result = hdl.get_recent_diagnostics("wid-001", time.time())

        assert result is None


# ================================================================
# build_digest
# ================================================================

class TestBuildDigest:
    def test_all_healthy(self):
        """Digest with all devices online and no faults."""
        health = [
            {"device_id": "SC-001", "online": True, "last_seen": "2026-02-17T12:00:00Z",
             "seconds_since_seen": 60, "app_version": 3, "recent_faults": []},
            {"device_id": "SC-002", "online": True, "last_seen": "2026-02-17T12:00:00Z",
             "seconds_since_seen": 120, "app_version": 3, "recent_faults": []},
        ]

        digest = hdl.build_digest(health)

        assert digest["total"] == 2
        assert digest["online"] == 2
        assert digest["offline"] == 0
        assert digest["faulted"] == 0
        assert "All devices healthy" in digest["body"]
        assert "2/2 online" in digest["subject"]

    def test_one_offline(self):
        """Digest correctly identifies offline device."""
        health = [
            {"device_id": "SC-001", "online": True, "last_seen": "2026-02-17T12:00:00Z",
             "seconds_since_seen": 60, "app_version": 3, "recent_faults": []},
            {"device_id": "SC-002", "online": False, "last_seen": "2026-02-17T10:00:00Z",
             "seconds_since_seen": 7200, "app_version": 3, "recent_faults": []},
        ]

        digest = hdl.build_digest(health)

        assert digest["total"] == 2
        assert digest["online"] == 1
        assert digest["offline"] == 1
        assert "OFFLINE DEVICES" in digest["body"]
        assert "SC-002" in digest["body"]
        assert "1/2 online" in digest["subject"]

    def test_faulted_device(self):
        """Digest correctly identifies faulted device."""
        health = [
            {"device_id": "SC-001", "online": True, "last_seen": "2026-02-17T12:00:00Z",
             "seconds_since_seen": 60, "app_version": 3,
             "recent_faults": ["fault_sensor", "fault_interlock"]},
        ]

        digest = hdl.build_digest(health)

        assert digest["faulted"] == 1
        assert "RECENT FAULTS" in digest["body"]
        assert "fault_sensor" in digest["body"]
        assert "1 faulted" in digest["subject"]

    def test_firmware_version_distribution(self):
        """Digest shows firmware version distribution."""
        health = [
            {"device_id": "SC-001", "online": True, "last_seen": "x",
             "seconds_since_seen": 60, "app_version": 3, "recent_faults": []},
            {"device_id": "SC-002", "online": True, "last_seen": "x",
             "seconds_since_seen": 60, "app_version": 2, "recent_faults": []},
        ]

        digest = hdl.build_digest(health)

        assert "Firmware versions:" in digest["body"]
        assert "v3: 1 device(s)" in digest["body"]
        assert "v2: 1 device(s)" in digest["body"]

    def test_empty_fleet(self):
        """Digest with no devices."""
        digest = hdl.build_digest([])

        assert digest["total"] == 0
        assert digest["online"] == 0
        assert "0/0 online" in digest["subject"]

    def test_with_diagnostics_responses(self):
        """Digest includes DIAGNOSTICS RESPONSES section."""
        health = [
            {"device_id": "SC-001", "online": False, "last_seen": "x",
             "seconds_since_seen": 7200, "app_version": 3, "recent_faults": []},
        ]
        diag_responses = {
            "SC-001": {
                "app_version": 3,
                "uptime_seconds": 97200,
                "boot_count": 5,
                "last_error_name": "sensor",
                "event_buffer_pending": 12,
            }
        }

        digest = hdl.build_digest(health, diag_responses)

        assert "DIAGNOSTICS RESPONSES" in digest["body"]
        assert "SC-001" in digest["body"]
        assert "v3" in digest["body"]
        assert "27.0h" in digest["body"]
        assert "boots 5" in digest["body"]
        assert "err=sensor" in digest["body"]
        assert "buf=12" in digest["body"]

    def test_no_diagnostics_responses(self):
        """Digest without diag_responses omits section."""
        health = [
            {"device_id": "SC-001", "online": True, "last_seen": "x",
             "seconds_since_seen": 60, "app_version": 3, "recent_faults": []},
        ]

        digest = hdl.build_digest(health)

        assert "DIAGNOSTICS RESPONSES" not in digest["body"]

    def test_diag_queried_count(self):
        """Digest includes diag_queried count."""
        health = []
        diag_responses = {
            "SC-001": {"app_version": 3, "uptime_seconds": 100,
                       "boot_count": 0, "last_error_name": "none",
                       "event_buffer_pending": 0},
            "SC-002": {"app_version": 3, "uptime_seconds": 200,
                       "boot_count": 1, "last_error_name": "none",
                       "event_buffer_pending": 0},
        }

        digest = hdl.build_digest(health, diag_responses)

        assert digest["diag_queried"] == 2


# ================================================================
# get_recent_faults
# ================================================================

class TestGetRecentFaults:
    def test_no_faults(self):
        """No fault flags in recent events returns empty list."""
        items = [make_event_item("wid-001", 1000000)]

        with patch.object(hdl.events_table, "query", return_value={"Items": items}):
            faults = hdl.get_recent_faults("wid-001", time.time())

        assert faults == []

    def test_multiple_faults(self):
        """Multiple fault types are collected and sorted."""
        items = [
            make_event_item("wid-001", 1000000, ["fault_interlock"]),
            make_event_item("wid-001", 1000001, ["fault_sensor"]),
        ]

        with patch.object(hdl.events_table, "query", return_value={"Items": items}):
            faults = hdl.get_recent_faults("wid-001", time.time())

        assert faults == ["fault_interlock", "fault_sensor"]

    def test_deduplicated(self):
        """Same fault in multiple events is only reported once."""
        items = [
            make_event_item("wid-001", 1000000, ["fault_sensor"]),
            make_event_item("wid-001", 1000001, ["fault_sensor"]),
        ]

        with patch.object(hdl.events_table, "query", return_value={"Items": items}):
            faults = hdl.get_recent_faults("wid-001", time.time())

        assert faults == ["fault_sensor"]

    def test_query_failure_returns_empty(self):
        """DynamoDB query failure returns empty list."""
        with patch.object(hdl.events_table, "query", side_effect=Exception("Timeout")):
            faults = hdl.get_recent_faults("wid-001", time.time())

        assert faults == []


# ================================================================
# lambda_handler
# ================================================================

class TestLambdaHandler:
    def test_no_devices(self):
        """Handler with empty registry returns early."""
        with patch.object(hdl, "get_all_active_devices", return_value=[]):
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200

    def test_full_flow(self):
        """Handler scans registry, checks health, publishes digest."""
        now = time.time()
        last_seen = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60))
        devices = [make_device("SC-001", last_seen=last_seen)]

        with patch.object(hdl, "get_all_active_devices", return_value=devices), \
             patch.object(hdl, "get_recent_faults", return_value=[]), \
             patch.object(hdl, "get_recent_diagnostics", return_value=None), \
             patch.object(hdl, "publish_digest") as mock_pub:
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200
        mock_pub.assert_called_once()
        digest = mock_pub.call_args[0][0]
        assert digest["total"] == 1
        assert digest["online"] == 1

    def test_no_sns_topic_skips_publish(self):
        """Handler skips SNS publish when topic ARN is empty."""
        hdl.SNS_TOPIC_ARN = ""
        now = time.time()
        last_seen = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60))
        devices = [make_device("SC-001", last_seen=last_seen)]

        with patch.object(hdl, "get_all_active_devices", return_value=devices), \
             patch.object(hdl, "get_recent_faults", return_value=[]), \
             patch.object(hdl, "get_recent_diagnostics", return_value=None), \
             patch.object(hdl.sns, "publish") as mock_sns:
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200
        mock_sns.assert_not_called()

    def test_auto_diag_disabled_skips_send(self):
        """When AUTO_DIAG_ENABLED is false, no diagnostic requests sent."""
        hdl.AUTO_DIAG_ENABLED = False
        now = time.time()
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 3600)
        )
        devices = [make_device("SC-001", last_seen=last_seen)]

        with patch.object(hdl, "get_all_active_devices", return_value=devices), \
             patch.object(hdl, "get_recent_faults", return_value=[]), \
             patch.object(hdl, "get_recent_diagnostics", return_value=None), \
             patch.object(hdl, "send_sidewalk_msg") as mock_send, \
             patch.object(hdl, "publish_digest"):
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200
        mock_send.assert_not_called()

    def test_auto_diag_enabled_sends_to_unhealthy(self):
        """When AUTO_DIAG_ENABLED is true, sends 0x40 to unhealthy devices."""
        hdl.AUTO_DIAG_ENABLED = True
        now = time.time()
        # Offline device
        last_seen = time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 3600)
        )
        devices = [make_device("SC-001", wireless_id="wid-001",
                               last_seen=last_seen)]

        with patch.object(hdl, "get_all_active_devices", return_value=devices), \
             patch.object(hdl, "get_recent_faults", return_value=[]), \
             patch.object(hdl, "get_recent_diagnostics", return_value=None), \
             patch.object(hdl, "send_sidewalk_msg") as mock_send, \
             patch.object(hdl, "publish_digest"):
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200
        mock_send.assert_called_once_with(
            bytes([0x40]), wireless_device_id="wid-001"
        )
        body = json.loads(result["body"])
        assert body["diag_queried"] == 1

    def test_response_includes_diag_counts(self):
        """Lambda response body includes diag_queried and diag_responses."""
        now = time.time()
        last_seen = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60))
        devices = [make_device("SC-001", last_seen=last_seen)]

        diag_data = {
            "app_version": 3,
            "uptime_seconds": 120,
            "boot_count": 0,
            "last_error_name": "none",
            "event_buffer_pending": 0,
        }

        with patch.object(hdl, "get_all_active_devices", return_value=devices), \
             patch.object(hdl, "get_recent_faults", return_value=[]), \
             patch.object(hdl, "get_recent_diagnostics",
                          return_value=diag_data), \
             patch.object(hdl, "publish_digest"):
            result = hdl.lambda_handler({}, None)

        body = json.loads(result["body"])
        assert body["diag_responses"] == 1


# Need json for TestLambdaHandler
import json  # noqa: E402
