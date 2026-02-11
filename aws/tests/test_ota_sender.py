"""Tests for ota_sender_lambda.py — OTA reliability fixes.

Covers:
- NO_SESSION (status=3) handling: re-sends OTA_START, caps at 3 restarts
- Delta retry index: uses delta_list[cursor] not raw next_chunk
- Stale "validating" retry: re-sends START instead of chunk
- Happy path: delta ACK sends correct next chunk
- Happy path: full-mode ACK sends correct next chunk
"""

import json
import os
import struct
import sys
from unittest.mock import MagicMock, patch

import pytest

# --- Module-level mocking ---
# ota_sender_lambda.py imports boto3 and sidewalk_utils at module level.
# We mock them before import so the module initializes cleanly.

mock_sidewalk_utils = MagicMock()
mock_sidewalk_utils.get_device_id.return_value = "test-device-id"
mock_sidewalk_utils.send_sidewalk_msg = MagicMock()

sys.modules["sidewalk_utils"] = mock_sidewalk_utils
sys.modules["boto3"] = MagicMock()

# Add aws/ to path so we can import the Lambda module
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import ota_sender_lambda as ota  # noqa: E402


# --- Test fixtures ---

FIRMWARE = b"\x45\x56\x53\x45" + b"\x00" * 56  # 60 bytes, starts with EVSE magic
FIRMWARE_CRC = ota.crc32(FIRMWARE)
CHUNK_SIZE = 15
FULL_CHUNKS = (len(FIRMWARE) + CHUNK_SIZE - 1) // CHUNK_SIZE  # 4 chunks


def make_delta_session(delta_chunks=None, **overrides):
    """Build a delta-mode OTA session dict."""
    if delta_chunks is None:
        delta_chunks = [1, 3]  # abs indices of changed chunks
    session = {
        "s3_bucket": "test-bucket",
        "s3_key": "firmware/app-v2.bin",
        "fw_size": len(FIRMWARE),
        "fw_crc32": FIRMWARE_CRC,
        "total_chunks": len(delta_chunks),
        "chunk_size": CHUNK_SIZE,
        "version": 2,
        "next_chunk": 0,
        "retries": 0,
        "status": "sending",
        "highest_acked": 0,
        "delta_chunks": json.dumps(delta_chunks),
        "delta_cursor": 0,
    }
    session.update(overrides)
    return session


def make_full_session(**overrides):
    """Build a full-mode OTA session dict."""
    session = {
        "s3_bucket": "test-bucket",
        "s3_key": "firmware/app-v2.bin",
        "fw_size": len(FIRMWARE),
        "fw_crc32": FIRMWARE_CRC,
        "total_chunks": FULL_CHUNKS,
        "chunk_size": CHUNK_SIZE,
        "version": 2,
        "next_chunk": 0,
        "retries": 0,
        "status": "sending",
        "highest_acked": 0,
    }
    session.update(overrides)
    return session


@pytest.fixture(autouse=True)
def reset_mocks():
    """Reset all mocks and caches before each test."""
    mock_sidewalk_utils.send_sidewalk_msg.reset_mock()
    ota._firmware_cache.clear()
    ota._firmware_cache[f"test-bucket/firmware/app-v2.bin"] = FIRMWARE
    yield


# --- Layer 3a: NO_SESSION handling ---

class TestNoSessionHandling:
    """NO_SESSION (status=3) should re-send OTA_START, not retry a chunk."""

    def test_no_session_resends_start(self):
        session = make_delta_session(delta_cursor=1)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session") as mock_write, \
             patch.object(ota, "send_sidewalk_msg") as mock_send:

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_NO_SESSION,
                "next_chunk": 1,
                "chunks_received": 1,
            })

        assert "resent START" in result["body"]
        # Verify OTA_START was sent (not a chunk)
        sent_msg = mock_send.call_args[0][0]
        assert sent_msg[0] == ota.OTA_CMD_TYPE
        assert sent_msg[1] == ota.OTA_SUB_START

        # Verify restarts counter incremented
        written = mock_write.call_args[0][0]
        assert written["restarts"] == 1
        assert written["status"] == "restarting"

    def test_no_session_caps_at_3_restarts(self):
        session = make_delta_session(restarts=3)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "clear_session") as mock_clear, \
             patch.object(ota, "log_ota_event") as mock_log:

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_NO_SESSION,
                "next_chunk": 0,
                "chunks_received": 0,
            })

        assert "aborted" in result["body"]
        mock_clear.assert_called_once()
        mock_log.assert_called_once()
        assert mock_log.call_args[0][0] == "ota_aborted"

    def test_no_session_increments_from_existing_restarts(self):
        session = make_delta_session(restarts=2)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session") as mock_write, \
             patch.object(ota, "send_sidewalk_msg"):

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_NO_SESSION,
                "next_chunk": 0,
                "chunks_received": 0,
            })

        assert "restart 3" in result["body"]
        written = mock_write.call_args[0][0]
        assert written["restarts"] == 3


# --- Layer 3b: Delta retry index fix ---

class TestDeltaRetryIndex:
    """Error retries in delta mode should use delta_list[cursor], not raw next_chunk."""

    def test_delta_error_retries_correct_abs_index(self):
        """When device NAKs with CRC_ERR in delta mode, retry the chunk at delta_list[cursor]."""
        delta_chunks = [5, 10, 42]  # abs indices
        session = make_delta_session(delta_chunks=delta_chunks, delta_cursor=1)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_chunk") as mock_send:

            ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_CRC_ERR,
                "next_chunk": 0,  # raw next_chunk (wrong if used directly)
                "chunks_received": 1,
            })

        # Should retry abs index 10 (delta_list[1]), NOT chunk 0
        mock_send.assert_called_once()
        sent_idx = mock_send.call_args[0][1]
        assert sent_idx == 10

    def test_delta_error_falls_back_when_cursor_past_end(self):
        """If cursor is past the delta list, fall back to next_chunk."""
        delta_chunks = [5]
        session = make_delta_session(delta_chunks=delta_chunks, delta_cursor=5)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_chunk") as mock_send:

            ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_FLASH_ERR,
                "next_chunk": 7,
                "chunks_received": 1,
            })

        sent_idx = mock_send.call_args[0][1]
        assert sent_idx == 7

    def test_full_mode_error_uses_next_chunk(self):
        """In full mode, retry uses raw next_chunk (no delta list)."""
        session = make_full_session(next_chunk=3)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_chunk") as mock_send:

            ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_CRC_ERR,
                "next_chunk": 3,
                "chunks_received": 3,
            })

        sent_idx = mock_send.call_args[0][1]
        assert sent_idx == 3


# --- Layer 3c: Stale "validating" retry ---

class TestValidatingRetry:
    """Stale session in 'validating' status should re-send START, not a chunk."""

    def test_validating_resends_start(self):
        session = make_delta_session(status="validating", updated_at=0)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_sidewalk_msg") as mock_send:

            result = ota.handle_retry_check({"source": "aws.events"})

        assert "retried" in result["body"]
        sent_msg = mock_send.call_args[0][0]
        assert sent_msg[0] == ota.OTA_CMD_TYPE
        assert sent_msg[1] == ota.OTA_SUB_START

    def test_starting_resends_start(self):
        session = make_full_session(status="starting", updated_at=0)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_sidewalk_msg") as mock_send:

            result = ota.handle_retry_check({"source": "aws.events"})

        sent_msg = mock_send.call_args[0][0]
        assert sent_msg[1] == ota.OTA_SUB_START

    def test_restarting_resends_start(self):
        session = make_full_session(status="restarting", updated_at=0)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_sidewalk_msg") as mock_send:

            result = ota.handle_retry_check({"source": "aws.events"})

        sent_msg = mock_send.call_args[0][0]
        assert sent_msg[1] == ota.OTA_SUB_START

    def test_sending_resends_chunk(self):
        """Stale 'sending' status should retry the chunk, not START."""
        session = make_full_session(status="sending", next_chunk=2, updated_at=0)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_chunk") as mock_send:

            result = ota.handle_retry_check({"source": "aws.events"})

        mock_send.assert_called_once()
        sent_idx = mock_send.call_args[0][1]
        assert sent_idx == 2

    def test_stale_max_retries_aborts(self):
        session = make_full_session(status="validating", retries=5, updated_at=0)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "clear_session") as mock_clear, \
             patch.object(ota, "log_ota_event"):

            result = ota.handle_retry_check({"source": "aws.events"})

        assert "aborted" in result["body"]
        mock_clear.assert_called_once()

    def test_fresh_session_not_retried(self):
        """Session updated recently should not trigger retry."""
        import time
        session = make_full_session(updated_at=int(time.time()))

        with patch.object(ota, "get_session", return_value=session):
            result = ota.handle_retry_check({"source": "aws.events"})

        assert "session active" in result["body"]


# --- Happy path: delta ACK flow ---

class TestDeltaAckHappyPath:
    """Verify delta ACK sends the correct next absolute chunk index."""

    def test_first_delta_ack_sends_first_chunk(self):
        delta_chunks = [5, 10]
        session = make_delta_session(delta_chunks=delta_chunks)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_chunk") as mock_send:

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_OK,
                "next_chunk": 0,
                "chunks_received": 0,
            })

        mock_send.assert_called_once()
        sent_idx = mock_send.call_args[0][1]
        assert sent_idx == 5  # delta_list[0]

    def test_second_delta_ack_sends_second_chunk(self):
        delta_chunks = [5, 10]
        session = make_delta_session(delta_chunks=delta_chunks)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_chunk") as mock_send:

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_OK,
                "next_chunk": 1,
                "chunks_received": 1,
            })

        sent_idx = mock_send.call_args[0][1]
        assert sent_idx == 10  # delta_list[1]

    def test_all_delta_chunks_acked_waits_for_complete(self):
        delta_chunks = [5]
        session = make_delta_session(delta_chunks=delta_chunks, highest_acked=0)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session") as mock_write:

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_OK,
                "next_chunk": 1,
                "chunks_received": 1,
            })

        assert "awaiting COMPLETE" in result["body"]
        written = mock_write.call_args[0][0]
        assert written["status"] == "validating"


# --- Happy path: full-mode ACK flow ---

class TestFullModeAckHappyPath:
    def test_ack_sends_next_chunk(self):
        session = make_full_session(next_chunk=0, highest_acked=0)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session"), \
             patch.object(ota, "send_chunk") as mock_send:

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_OK,
                "next_chunk": 1,
                "chunks_received": 1,
            })

        sent_idx = mock_send.call_args[0][1]
        assert sent_idx == 1

    def test_all_chunks_acked_waits_for_complete(self):
        session = make_full_session(next_chunk=3, highest_acked=3)

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "write_session") as mock_write:

            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_OK,
                "next_chunk": FULL_CHUNKS,
                "chunks_received": FULL_CHUNKS,
            })

        assert "awaiting COMPLETE" in result["body"]


# --- COMPLETE handler ---

class TestDeviceComplete:
    def test_success_clears_session(self):
        session = make_full_session()

        with patch.object(ota, "get_session", return_value=session), \
             patch.object(ota, "clear_session") as mock_clear, \
             patch.object(ota, "log_ota_event"), \
             patch.object(ota.s3, "copy_object"):

            result = ota.handle_device_complete({
                "type": "complete",
                "result": ota.OTA_STATUS_OK,
                "crc32_calc": f"0x{FIRMWARE_CRC:08x}",
            })

        assert "result=0" in result["body"]
        mock_clear.assert_called_once()

    def test_no_session_still_logs(self):
        with patch.object(ota, "get_session", return_value=None), \
             patch.object(ota, "clear_session"), \
             patch.object(ota, "log_ota_event") as mock_log:

            result = ota.handle_device_complete({
                "type": "complete",
                "result": ota.OTA_STATUS_OK,
            })

        mock_log.assert_called_once()


# --- No active session ---

class TestNoActiveSession:
    def test_ack_with_no_session_ignored(self):
        with patch.object(ota, "get_session", return_value=None):
            result = ota.handle_device_ack({
                "type": "ack",
                "status": ota.OTA_STATUS_OK,
                "next_chunk": 0,
                "chunks_received": 0,
            })
        assert "no session" in result["body"]

    def test_retry_with_no_session_ignored(self):
        with patch.object(ota, "get_session", return_value=None):
            result = ota.handle_retry_check({"source": "aws.events"})
        assert "no active session" in result["body"]
