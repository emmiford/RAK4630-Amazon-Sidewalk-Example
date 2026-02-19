"""Tests for cmd_auth.py — command authentication signing."""

import hashlib
import hmac
import os
import struct
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from cmd_auth import CMD_AUTH_KEY_SIZE, CMD_AUTH_TAG_SIZE, get_auth_key, sign_command


# --- Test key (same as C tests: 32 bytes of 0xAA) ---
TEST_KEY = bytes([0xAA] * 32)


class TestSignCommand:
    def test_tag_length(self):
        payload = bytes([0x10, 0x01, 0x00, 0x00])
        tag = sign_command(payload, TEST_KEY)
        assert len(tag) == CMD_AUTH_TAG_SIZE

    def test_legacy_allow_known_vector(self):
        """Verify against known HMAC-SHA256 output (matches C test vectors)."""
        payload = bytes([0x10, 0x01, 0x00, 0x00])
        tag = sign_command(payload, TEST_KEY)
        assert tag == bytes.fromhex("0ae1ce9ff290071d")

    def test_legacy_pause_known_vector(self):
        payload = bytes([0x10, 0x00, 0x00, 0x00])
        tag = sign_command(payload, TEST_KEY)
        assert tag == bytes.fromhex("08847a9eab15b67e")

    def test_delay_window_known_vector(self):
        payload = bytearray(10)
        payload[0] = 0x10
        payload[1] = 0x02
        struct.pack_into("<I", payload, 2, 1000)
        struct.pack_into("<I", payload, 6, 2800)
        tag = sign_command(bytes(payload), TEST_KEY)
        assert tag == bytes.fromhex("e3ae1fa515664708")

    def test_different_keys_produce_different_tags(self):
        payload = bytes([0x10, 0x01, 0x00, 0x00])
        key_a = bytes([0xAA] * 32)
        key_b = bytes([0xBB] * 32)
        assert sign_command(payload, key_a) != sign_command(payload, key_b)

    def test_different_payloads_produce_different_tags(self):
        tag_allow = sign_command(bytes([0x10, 0x01, 0x00, 0x00]), TEST_KEY)
        tag_pause = sign_command(bytes([0x10, 0x00, 0x00, 0x00]), TEST_KEY)
        assert tag_allow != tag_pause

    def test_matches_stdlib_hmac(self):
        """Cross-check: our function matches direct hmac.new call."""
        payload = bytes([0x10, 0x02, 0x01, 0x02, 0x03, 0x04,
                         0x05, 0x06, 0x07, 0x08])
        expected = hmac.new(TEST_KEY, payload, hashlib.sha256).digest()[:8]
        assert sign_command(payload, TEST_KEY) == expected


class TestMtuCompliance:
    """Verify signed payloads fit within 19-byte LoRa downlink MTU."""

    def test_legacy_charge_control_fits(self):
        # Legacy: 4-byte payload + 8-byte tag = 12 bytes
        payload = bytes([0x10, 0x01, 0x00, 0x00])
        tag = sign_command(payload, TEST_KEY)
        total = len(payload) + len(tag)
        assert total <= 19, f"Legacy + tag = {total} bytes, exceeds 19-byte MTU"

    def test_delay_window_fits(self):
        # Delay window: 10-byte payload + 8-byte tag = 18 bytes
        payload = bytearray(10)
        payload[0] = 0x10
        payload[1] = 0x02
        struct.pack_into("<I", payload, 2, 1000)
        struct.pack_into("<I", payload, 6, 2800)
        tag = sign_command(bytes(payload), TEST_KEY)
        total = len(payload) + len(tag)
        assert total <= 19, f"Delay window + tag = {total} bytes, exceeds 19-byte MTU"


class TestGetAuthKey:
    def test_returns_none_when_not_set(self, monkeypatch):
        monkeypatch.delenv("CMD_AUTH_KEY", raising=False)
        assert get_auth_key() is None

    def test_returns_key_from_env(self, monkeypatch):
        hex_key = "aa" * 32
        monkeypatch.setenv("CMD_AUTH_KEY", hex_key)
        key = get_auth_key()
        assert key == bytes.fromhex(hex_key)
        assert len(key) == CMD_AUTH_KEY_SIZE

    def test_raises_on_wrong_length(self, monkeypatch):
        monkeypatch.setenv("CMD_AUTH_KEY", "aabb")  # only 2 bytes
        with pytest.raises(ValueError, match="must be 32 bytes"):
            get_auth_key()
