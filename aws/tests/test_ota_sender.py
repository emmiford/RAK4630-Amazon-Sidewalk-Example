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
    ota._firmware_cache["test-bucket/firmware/app-v2.bin"] = FIRMWARE
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


# --- compute_delta_chunks edge cases ---

class TestComputeDeltaChunks:
    """Edge cases for compute_delta_chunks(): baseline/firmware size mismatches,
    identical images, and single-byte differences."""

    def test_baseline_larger_than_firmware_pads_with_0xff(self):
        """When baseline is larger than firmware, extra baseline bytes beyond
        firmware length are irrelevant — we only compare up to firmware chunks.
        Chunks where baseline extends but firmware is shorter should treat the
        baseline suffix as 0xFF-padded for comparison against the (shorter)
        firmware chunk."""
        chunk_size = 4
        baseline = b"\x01\x02\x03\x04" + b"\x05\x06\x07\x08"  # 8 bytes, 2 chunks
        firmware = b"\x01\x02\x03\x04"  # 4 bytes, 1 chunk — identical first chunk

        changed = ota.compute_delta_chunks(baseline, firmware, chunk_size)

        # Only 1 firmware chunk; it matches baseline chunk 0, so 0 changes
        assert changed == []

    def test_baseline_larger_firmware_differs_in_first_chunk(self):
        """Baseline larger than firmware, but first chunk differs."""
        chunk_size = 4
        baseline = b"\x01\x02\x03\x04" + b"\x05\x06\x07\x08"
        firmware = b"\xFF\x02\x03\x04"  # differs in byte 0

        changed = ota.compute_delta_chunks(baseline, firmware, chunk_size)
        assert changed == [0]

    def test_empty_baseline_all_chunks_changed(self):
        """Empty baseline means no old data — all firmware chunks are new.
        Each firmware chunk compared against empty (0xFF-padded), so any
        non-0xFF data marks the chunk as changed."""
        chunk_size = 4
        baseline = b""
        firmware = b"\x01\x02\x03\x04\x05\x06\x07\x08"  # 2 chunks

        changed = ota.compute_delta_chunks(baseline, firmware, chunk_size)

        # Both chunks differ from the 0xFF pad
        assert changed == [0, 1]

    def test_empty_baseline_firmware_all_0xff_no_changes(self):
        """Edge case: empty baseline + firmware that is all 0xFF.
        Padding with 0xFF makes them match."""
        chunk_size = 4
        baseline = b""
        firmware = b"\xff\xff\xff\xff"

        changed = ota.compute_delta_chunks(baseline, firmware, chunk_size)
        assert changed == []

    def test_identical_baseline_and_firmware_zero_changes(self):
        """Identical images produce zero changed chunks."""
        chunk_size = 4
        image = b"\x10\x20\x30\x40\x50\x60\x70\x80"

        changed = ota.compute_delta_chunks(image, image, chunk_size)
        assert changed == []

    def test_single_byte_difference_in_one_chunk(self):
        """One byte different in chunk 1 should flag only chunk 1."""
        chunk_size = 4
        baseline = b"\xAA\xBB\xCC\xDD" + b"\x11\x22\x33\x44"
        firmware = b"\xAA\xBB\xCC\xDD" + b"\x11\x22\x33\x45"  # last byte differs

        changed = ota.compute_delta_chunks(baseline, firmware, chunk_size)
        assert changed == [1]

    def test_partial_last_chunk_pads_baseline(self):
        """Firmware whose last chunk is partial — baseline should be padded
        with 0xFF to match the shorter firmware chunk length."""
        chunk_size = 4
        baseline = b"\x01\x02\x03\x04\x05\x06"  # 6 bytes → 2 chunks (4 + 2)
        firmware = b"\x01\x02\x03\x04\x05\x06"  # identical

        changed = ota.compute_delta_chunks(baseline, firmware, chunk_size)
        assert changed == []

    def test_many_chunks_sparse_changes(self):
        """Larger image with changes only in specific chunks."""
        chunk_size = 4
        baseline = bytes(range(40))  # 10 chunks
        fw = bytearray(baseline)
        fw[0] = 0xFF   # change chunk 0
        fw[16] = 0xFF  # change chunk 4
        fw[36] = 0xFF  # change chunk 9

        changed = ota.compute_delta_chunks(baseline, bytes(fw), chunk_size)
        assert changed == [0, 4, 9]


# --- build_ota_chunk format ---

class TestBuildOtaChunk:
    """Verify the wire format of build_ota_chunk(): cmd=0x20, sub=0x02,
    index as 2-byte little-endian, then raw data."""

    def test_chunk_format_basic(self):
        data = b"\xDE\xAD\xBE\xEF"
        msg = ota.build_ota_chunk(0, data)

        assert msg[0] == 0x20            # OTA_CMD_TYPE
        assert msg[1] == 0x02            # OTA_SUB_CHUNK
        assert msg[2] == 0x00            # chunk_idx low byte
        assert msg[3] == 0x00            # chunk_idx high byte
        assert msg[4:] == data           # payload

    def test_chunk_index_little_endian(self):
        """Chunk index 0x0102 should be encoded as [0x02, 0x01] (little-endian)."""
        data = b"\xFF"
        msg = ota.build_ota_chunk(0x0102, data)

        assert msg[2] == 0x02  # low byte
        assert msg[3] == 0x01  # high byte

    def test_chunk_index_max_practical(self):
        """Test with a large index (1023 — max for 128-byte bitmap)."""
        data = b"\x42"
        msg = ota.build_ota_chunk(1023, data)

        assert msg[2] == 0xFF  # 1023 & 0xFF
        assert msg[3] == 0x03  # 1023 >> 8

    def test_chunk_total_length(self):
        """Total message should be 4-byte header + data length."""
        data = bytes(range(15))  # 15 bytes (max LoRa chunk)
        msg = ota.build_ota_chunk(0, data)

        assert len(msg) == 4 + 15  # 19 bytes (full LoRa MTU)

    def test_chunk_data_passthrough(self):
        """Data bytes should appear unchanged after the 4-byte header."""
        data = bytes(range(256))  # larger than LoRa, still valid structurally
        msg = ota.build_ota_chunk(42, data)

        assert msg[4:] == data
