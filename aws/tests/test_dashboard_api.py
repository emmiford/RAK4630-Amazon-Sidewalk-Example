"""Tests for dashboard_api_lambda.py — fleet dashboard REST API."""

import json
import os
import sys
import time
from datetime import datetime
from decimal import Decimal
from unittest.mock import MagicMock, patch
from zoneinfo import ZoneInfo

import pytest

# Module-level mocking
if "boto3" not in sys.modules:
    sys.modules["boto3"] = MagicMock()

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import dashboard_api_lambda as dashboard  # noqa: E402

MT = ZoneInfo("America/Denver")


def parse_body(response):
    """Parse the JSON body from an API response."""
    return json.loads(response["body"])


# --- Auth ---

class TestAuth:
    def test_no_key_configured_allows_all(self):
        with patch.object(dashboard, "DASHBOARD_API_KEY", ""):
            result = dashboard.check_auth({"headers": {}})
        assert result is None

    def test_valid_key_passes(self):
        with patch.object(dashboard, "DASHBOARD_API_KEY", "secret-key"):
            result = dashboard.check_auth({"headers": {"x-api-key": "secret-key"}})
        assert result is None

    def test_invalid_key_returns_401(self):
        with patch.object(dashboard, "DASHBOARD_API_KEY", "secret-key"):
            result = dashboard.check_auth({"headers": {"x-api-key": "wrong"}})
        assert result["statusCode"] == 401

    def test_missing_key_returns_401(self):
        with patch.object(dashboard, "DASHBOARD_API_KEY", "secret-key"):
            result = dashboard.check_auth({"headers": {}})
        assert result["statusCode"] == 401


# --- DecimalEncoder ---

class TestDecimalEncoder:
    def test_decimal_to_float(self):
        data = {"value": Decimal("3.14")}
        result = json.dumps(data, cls=dashboard.DecimalEncoder)
        assert '"value": 3.14' in result

    def test_nested_decimal(self):
        data = {"items": [{"kwh": Decimal("12.345")}]}
        result = json.dumps(data, cls=dashboard.DecimalEncoder)
        parsed = json.loads(result)
        assert parsed["items"][0]["kwh"] == 12.345


# --- GET /devices ---

class TestGetDevices:
    def test_returns_device_list(self):
        mock_reg = MagicMock()
        mock_reg.scan.return_value = {
            "Items": [
                {"device_id": "SC-AABBCCDD", "status": "active",
                 "wireless_device_id": "uuid-1"},
            ]
        }
        mock_state = MagicMock()
        mock_state.scan.return_value = {
            "Items": [
                {"device_id": "SC-AABBCCDD", "last_seen": "2026-02-21 14:00:00.000",
                 "j1772_state": "B", "current_draw_ma": 0, "rssi": -75},
            ]
        }

        with patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "state_table", mock_state):
            result = dashboard.get_devices()

        assert result["statusCode"] == 200
        body = parse_body(result)
        assert body["count"] == 1
        dev = body["devices"][0]
        assert dev["device_id"] == "SC-AABBCCDD"
        assert dev["j1772_state"] == "B"
        assert dev["rssi"] == -75

    def test_empty_fleet(self):
        mock_reg = MagicMock()
        mock_reg.scan.return_value = {"Items": []}
        mock_state = MagicMock()
        mock_state.scan.return_value = {"Items": []}

        with patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "state_table", mock_state):
            result = dashboard.get_devices()

        body = parse_body(result)
        assert body["count"] == 0
        assert body["devices"] == []

    def test_online_status_recent(self):
        """Device seen within 2x heartbeat is online."""
        now = time.time()
        recent_mt = datetime.fromtimestamp(now - 60, tz=MT)
        recent_str = recent_mt.strftime("%Y-%m-%d %H:%M:%S") + ".000"

        mock_reg = MagicMock()
        mock_reg.scan.return_value = {
            "Items": [{"device_id": "SC-11111111", "status": "active"}]
        }
        mock_state = MagicMock()
        mock_state.scan.return_value = {
            "Items": [{"device_id": "SC-11111111", "last_seen": recent_str}]
        }

        with patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "state_table", mock_state):
            result = dashboard.get_devices()

        body = parse_body(result)
        assert body["devices"][0]["online"] is True

    def test_online_status_stale(self):
        """Device not seen for >2x heartbeat is offline."""
        now = time.time()
        old_mt = datetime.fromtimestamp(now - 7200, tz=MT)
        old_str = old_mt.strftime("%Y-%m-%d %H:%M:%S") + ".000"

        mock_reg = MagicMock()
        mock_reg.scan.return_value = {
            "Items": [{"device_id": "SC-22222222", "status": "active"}]
        }
        mock_state = MagicMock()
        mock_state.scan.return_value = {
            "Items": [{"device_id": "SC-22222222", "last_seen": old_str}]
        }

        with patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "state_table", mock_state):
            result = dashboard.get_devices()

        body = parse_body(result)
        assert body["devices"][0]["online"] is False


# --- GET /devices/{id} ---

class TestGetDeviceDetail:
    def test_returns_device_with_events(self):
        mock_state = MagicMock()
        mock_state.get_item.return_value = {
            "Item": {"device_id": "SC-AABB", "j1772_state": "C"}
        }
        mock_reg = MagicMock()
        mock_reg.get_item.return_value = {
            "Item": {"device_id": "SC-AABB", "status": "active"}
        }
        mock_events = MagicMock()
        mock_events.query.return_value = {
            "Items": [
                {"device_id": "SC-AABB", "timestamp_mt": "2026-02-21 14:00:00.000",
                 "event_type": "evse_telemetry",
                 "data": {"evse": {"pilot_state": "C", "current_draw_ma": 24000,
                                   "pilot_voltage_mv": 1489}}},
            ]
        }

        with patch.object(dashboard, "state_table", mock_state), \
             patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "events_table", mock_events):
            result = dashboard.get_device_detail("SC-AABB", "1h")

        assert result["statusCode"] == 200
        body = parse_body(result)
        assert body["device_id"] == "SC-AABB"
        assert body["event_count"] == 1
        assert body["events"][0]["event_type"] == "evse_telemetry"

    def test_device_not_found(self):
        mock_state = MagicMock()
        mock_state.get_item.return_value = {}
        mock_reg = MagicMock()
        mock_reg.get_item.return_value = {}

        with patch.object(dashboard, "state_table", mock_state), \
             patch.object(dashboard, "registry_table", mock_reg):
            result = dashboard.get_device_detail("SC-NOTFOUND")

        assert result["statusCode"] == 404

    def test_event_type_filter(self):
        """Passing event_type should add FilterExpression."""
        mock_state = MagicMock()
        mock_state.get_item.return_value = {"Item": {"device_id": "SC-AABB"}}
        mock_reg = MagicMock()
        mock_reg.get_item.return_value = {"Item": {"device_id": "SC-AABB"}}
        mock_events = MagicMock()
        mock_events.query.return_value = {"Items": []}

        with patch.object(dashboard, "state_table", mock_state), \
             patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "events_table", mock_events):
            dashboard.get_device_detail("SC-AABB", "1h", event_type="ota_start")

        query_kwargs = mock_events.query.call_args[1]
        assert "#et" in query_kwargs["ExpressionAttributeNames"]
        assert query_kwargs["ExpressionAttributeValues"][":et"] == "ota_start"


# --- Event summary ---

class TestSummarizeEvent:
    def test_telemetry_summary(self):
        event = {
            "event_type": "evse_telemetry",
            "timestamp_mt": "2026-02-21 14:00:00.000",
            "data": {
                "evse": {
                    "pilot_state": "C",
                    "current_draw_ma": 24000,
                    "pilot_voltage_mv": 1489,
                    "thermostat_cool_active": True,
                }
            },
        }
        result = dashboard.summarize_event(event)
        assert "State C" in result["summary"]
        assert "24000mA" in result["summary"]
        assert "COOL" in result["summary"]
        assert result["anomaly"] is False

    def test_telemetry_with_fault(self):
        event = {
            "event_type": "evse_telemetry",
            "timestamp_mt": "2026-02-21 14:00:00.000",
            "data": {"evse": {"pilot_state": "A", "fault_sensor": True,
                               "current_draw_ma": 0, "pilot_voltage_mv": 0}},
        }
        result = dashboard.summarize_event(event)
        assert result["anomaly"] is True
        assert "FAULT:SENSOR" in result["summary"]

    def test_diagnostics_summary(self):
        event = {
            "event_type": "device_diagnostics",
            "timestamp_mt": "2026-02-21 14:00:00.000",
            "data": {"diagnostics": {"app_version": 10, "uptime_seconds": 3600,
                                      "last_error_name": "none"}},
        }
        result = dashboard.summarize_event(event)
        assert "v10" in result["summary"]
        assert "uptime=3600s" in result["summary"]
        assert result["anomaly"] is False

    def test_diagnostics_with_error(self):
        event = {
            "event_type": "device_diagnostics",
            "timestamp_mt": "2026-02-21 14:00:00.000",
            "data": {"diagnostics": {"app_version": 10, "uptime_seconds": 100,
                                      "last_error_name": "sensor"}},
        }
        result = dashboard.summarize_event(event)
        assert result["anomaly"] is True

    def test_scheduler_summary(self):
        event = {
            "event_type": "scheduler_command",
            "timestamp_mt": "2026-02-21 18:00:00.000",
            "command": "delay_window",
            "reason": "tou_peak",
        }
        result = dashboard.summarize_event(event)
        assert "delay_window" in result["summary"]
        assert "tou_peak" in result["summary"]

    def test_ota_summary(self):
        event = {
            "event_type": "ota_start",
            "timestamp_mt": "2026-02-21 14:00:00.000",
            "fw_size": 4096,
            "version": 5,
        }
        result = dashboard.summarize_event(event)
        assert "ota_start" in result["summary"]
        assert "v5" in result["summary"]

    def test_interlock_summary(self):
        event = {
            "event_type": "interlock_transition",
            "timestamp_mt": "2026-02-21 14:00:00.000",
            "transition_reason": "cloud_cmd",
            "charge_allowed": False,
        }
        result = dashboard.summarize_event(event)
        assert "cloud_cmd" in result["summary"]


# --- GET /devices/{id}/daily ---

class TestGetDeviceDaily:
    def test_returns_daily_records(self):
        mock_daily = MagicMock()
        mock_daily.query.return_value = {
            "Items": [
                {"device_id": "SC-AABB", "date": "2026-02-20",
                 "total_kwh": Decimal("12.5"), "event_count": 96},
                {"device_id": "SC-AABB", "date": "2026-02-19",
                 "total_kwh": Decimal("8.3"), "event_count": 90},
            ]
        }

        with patch.object(dashboard, "daily_stats_table", mock_daily):
            result = dashboard.get_device_daily("SC-AABB", days=30)

        assert result["statusCode"] == 200
        body = parse_body(result)
        assert body["count"] == 2
        assert body["records"][0]["total_kwh"] == 12.5


# --- GET /ota ---

class TestGetOtaActivity:
    def test_queries_gsi(self):
        mock_events = MagicMock()
        mock_events.query.return_value = {
            "Items": [
                {"device_id": "SC-AABB", "event_type": "ota_start",
                 "timestamp_mt": "2026-02-21 14:00:00.000",
                 "fw_size": 4096, "version": 5},
            ]
        }

        with patch.object(dashboard, "events_table", mock_events):
            result = dashboard.get_ota_activity(limit=10)

        assert result["statusCode"] == 200
        # Should query the GSI
        call_kwargs = mock_events.query.call_args_list[0][1]
        assert call_kwargs["IndexName"] == "event-type-index"


# --- Router ---

class TestRouter:
    def test_get_devices_route(self):
        mock_reg = MagicMock()
        mock_reg.scan.return_value = {"Items": []}
        mock_state = MagicMock()
        mock_state.scan.return_value = {"Items": []}

        event = {
            "requestContext": {"http": {"method": "GET"}},
            "rawPath": "/devices",
            "queryStringParameters": None,
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", ""), \
             patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "state_table", mock_state):
            result = dashboard.lambda_handler(event, None)

        assert result["statusCode"] == 200

    def test_get_device_detail_route(self):
        mock_state = MagicMock()
        mock_state.get_item.return_value = {"Item": {"device_id": "SC-AABB"}}
        mock_reg = MagicMock()
        mock_reg.get_item.return_value = {"Item": {"device_id": "SC-AABB"}}
        mock_events = MagicMock()
        mock_events.query.return_value = {"Items": []}

        event = {
            "requestContext": {"http": {"method": "GET"}},
            "rawPath": "/devices/SC-AABB",
            "queryStringParameters": {"window": "4h"},
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", ""), \
             patch.object(dashboard, "state_table", mock_state), \
             patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "events_table", mock_events):
            result = dashboard.lambda_handler(event, None)

        assert result["statusCode"] == 200

    def test_post_method_rejected(self):
        event = {
            "requestContext": {"http": {"method": "POST"}},
            "rawPath": "/devices",
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", ""):
            result = dashboard.lambda_handler(event, None)

        assert result["statusCode"] == 405

    def test_not_found_route(self):
        event = {
            "requestContext": {"http": {"method": "GET"}},
            "rawPath": "/unknown",
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", ""):
            result = dashboard.lambda_handler(event, None)

        assert result["statusCode"] == 404

    def test_auth_blocks_without_key(self):
        event = {
            "requestContext": {"http": {"method": "GET"}},
            "rawPath": "/devices",
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", "secret"):
            result = dashboard.lambda_handler(event, None)

        assert result["statusCode"] == 401

    def test_cors_headers(self):
        mock_reg = MagicMock()
        mock_reg.scan.return_value = {"Items": []}
        mock_state = MagicMock()
        mock_state.scan.return_value = {"Items": []}

        event = {
            "requestContext": {"http": {"method": "GET"}},
            "rawPath": "/devices",
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", ""), \
             patch.object(dashboard, "registry_table", mock_reg), \
             patch.object(dashboard, "state_table", mock_state):
            result = dashboard.lambda_handler(event, None)

        assert result["headers"]["Access-Control-Allow-Origin"] == "*"

    def test_daily_route(self):
        mock_daily = MagicMock()
        mock_daily.query.return_value = {"Items": []}

        event = {
            "requestContext": {"http": {"method": "GET"}},
            "rawPath": "/devices/SC-AABB/daily",
            "queryStringParameters": {"days": "7"},
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", ""), \
             patch.object(dashboard, "daily_stats_table", mock_daily):
            result = dashboard.lambda_handler(event, None)

        assert result["statusCode"] == 200

    def test_ota_route(self):
        mock_events = MagicMock()
        mock_events.query.return_value = {"Items": []}

        event = {
            "requestContext": {"http": {"method": "GET"}},
            "rawPath": "/ota",
            "queryStringParameters": None,
            "headers": {},
        }

        with patch.object(dashboard, "DASHBOARD_API_KEY", ""), \
             patch.object(dashboard, "events_table", mock_events):
            result = dashboard.lambda_handler(event, None)

        assert result["statusCode"] == 200
