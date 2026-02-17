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
    hdl.heartbeat_interval_s = 900
    hdl.sns_topic_arn = "arn:aws:sns:us-east-1:123456:test-topic"


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
        threshold = hdl.heartbeat_interval_s * hdl.OFFLINE_THRESHOLD_MULTIPLIER
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
        threshold = hdl.heartbeat_interval_s * hdl.OFFLINE_THRESHOLD_MULTIPLIER
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
        with patch.object(hdl, "get_all_devices", return_value=[]):
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200

    def test_full_flow(self):
        """Handler scans registry, checks health, publishes digest."""
        now = time.time()
        last_seen = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60))
        devices = [make_device("SC-001", last_seen=last_seen)]

        with patch.object(hdl, "get_all_devices", return_value=devices), \
             patch.object(hdl, "get_recent_faults", return_value=[]), \
             patch.object(hdl, "publish_digest") as mock_pub:
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200
        mock_pub.assert_called_once()
        digest = mock_pub.call_args[0][0]
        assert digest["total"] == 1
        assert digest["online"] == 1

    def test_no_sns_topic_skips_publish(self):
        """Handler skips SNS publish when topic ARN is empty."""
        hdl.sns_topic_arn = ""
        now = time.time()
        last_seen = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(now - 60))
        devices = [make_device("SC-001", last_seen=last_seen)]

        with patch.object(hdl, "get_all_devices", return_value=devices), \
             patch.object(hdl, "get_recent_faults", return_value=[]), \
             patch.object(hdl.sns, "publish") as mock_sns:
            result = hdl.lambda_handler({}, None)

        assert result["statusCode"] == 200
        mock_sns.assert_not_called()
