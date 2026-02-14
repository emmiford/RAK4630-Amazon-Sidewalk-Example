"""Tests for device_registry.py — SC short ID generation, auto-provisioning, last-seen updates."""

import os
import sys
from unittest.mock import MagicMock, patch

import pytest

# Ensure aws/ is on the path (conftest.py also does this, belt-and-suspenders)
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import device_registry as reg  # noqa: E402


# --- SC Short ID generation ---

class TestGenerateScShortId:
    def test_deterministic(self):
        """Same input always produces the same short ID."""
        id1 = reg.generate_sc_short_id("b319d001-6b08-4d88-b4ca-4d2d98a6d43c")
        id2 = reg.generate_sc_short_id("b319d001-6b08-4d88-b4ca-4d2d98a6d43c")
        assert id1 == id2

    def test_format(self):
        """Short ID matches SC-XXXXXXXX pattern (8 uppercase hex chars)."""
        sc_id = reg.generate_sc_short_id("any-device-id")
        assert sc_id.startswith("SC-")
        hex_part = sc_id[3:]
        assert len(hex_part) == 8
        # Must be valid hex
        int(hex_part, 16)

    def test_uppercase(self):
        """Hex portion is uppercase."""
        sc_id = reg.generate_sc_short_id("test-device")
        assert sc_id[3:] == sc_id[3:].upper()

    def test_different_inputs_differ(self):
        """Different device IDs produce different short IDs."""
        id1 = reg.generate_sc_short_id("device-aaa")
        id2 = reg.generate_sc_short_id("device-bbb")
        assert id1 != id2

    def test_known_value(self):
        """Verify against a pre-computed SHA-256."""
        import hashlib
        device_id = "b319d001-6b08-4d88-b4ca-4d2d98a6d43c"
        expected_hex = hashlib.sha256(device_id.encode()).hexdigest()[:8].upper()
        assert reg.generate_sc_short_id(device_id) == f"SC-{expected_hex}"


# --- get_or_create_device ---

class TestGetOrCreateDevice:
    def setup_method(self):
        self.table = MagicMock()
        self.device_id = "test-wireless-id-123"
        self.sc_id = reg.generate_sc_short_id(self.device_id)

    def test_existing_device_returned(self):
        """If device exists, return it without writing."""
        existing = {"device_id": self.sc_id, "status": "active"}
        self.table.get_item.return_value = {"Item": existing}

        result = reg.get_or_create_device(self.table, self.device_id, "sid-abc")
        assert result == existing
        self.table.put_item.assert_not_called()

    def test_new_device_created(self):
        """If device not found, auto-provision and return new record."""
        self.table.get_item.return_value = {}  # No 'Item' key

        result = reg.get_or_create_device(self.table, self.device_id, "sid-abc")
        assert result["device_id"] == self.sc_id
        assert result["wireless_device_id"] == self.device_id
        assert result["sidewalk_id"] == "sid-abc"
        assert result["status"] == "active"
        assert result["app_version"] == 0
        assert result["created_at"] != ""
        self.table.put_item.assert_called_once()

    def test_new_device_defaults(self):
        """New device has empty owner_email and location."""
        self.table.get_item.return_value = {}

        result = reg.get_or_create_device(self.table, self.device_id)
        assert result["owner_email"] == ""
        assert result["location"] == ""
        assert result["sidewalk_id"] == ""


# --- update_last_seen ---

class TestUpdateLastSeen:
    def setup_method(self):
        self.table = MagicMock()
        self.device_id = "test-wireless-id-456"
        self.sc_id = reg.generate_sc_short_id(self.device_id)

    def test_updates_timestamp(self):
        """Calls update_item with last_seen."""
        reg.update_last_seen(self.table, self.device_id)

        self.table.update_item.assert_called_once()
        call_kwargs = self.table.update_item.call_args[1]
        assert call_kwargs["Key"] == {"device_id": self.sc_id}
        assert ":ts" in call_kwargs["ExpressionAttributeValues"]
        assert "last_seen" in call_kwargs["UpdateExpression"]

    def test_updates_app_version(self):
        """When app_version provided, includes it in the update."""
        reg.update_last_seen(self.table, self.device_id, app_version=7)

        call_kwargs = self.table.update_item.call_args[1]
        assert "app_version" in call_kwargs["UpdateExpression"]
        assert call_kwargs["ExpressionAttributeValues"][":ver"] == 7

    def test_no_app_version_omitted(self):
        """When app_version is None, update only touches last_seen."""
        reg.update_last_seen(self.table, self.device_id)

        call_kwargs = self.table.update_item.call_args[1]
        assert "app_version" not in call_kwargs["UpdateExpression"]
        assert ":ver" not in call_kwargs["ExpressionAttributeValues"]

    def test_correct_sc_id_used(self):
        """The SC short ID in the Key matches what generate_sc_short_id returns."""
        reg.update_last_seen(self.table, self.device_id)

        call_kwargs = self.table.update_item.call_args[1]
        assert call_kwargs["Key"]["device_id"] == self.sc_id
