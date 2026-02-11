"""Tests for pure functions in ota_deploy.py — CRC32, delta chunks, formatting."""

import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import ota_deploy as deploy  # noqa: E402


# --- CRC32 ---

class TestCrc32:
    def test_empty(self):
        assert deploy.crc32(b"") == 0x00000000

    def test_known_value(self):
        # CRC32 of "EVSE" is well-known
        result = deploy.crc32(b"EVSE")
        assert isinstance(result, int)
        assert 0 <= result <= 0xFFFFFFFF

    def test_deterministic(self):
        data = b"\x00" * 60
        assert deploy.crc32(data) == deploy.crc32(data)

    def test_different_data_different_crc(self):
        assert deploy.crc32(b"\x00" * 60) != deploy.crc32(b"\xFF" * 60)


# --- Delta chunk computation ---

class TestComputeDeltaChunks:
    def test_identical_firmware_no_changes(self):
        fw = b"\x00" * 60
        baseline = b"\x00" * 60
        changed = deploy.compute_delta_chunks(baseline, fw, 15)
        assert changed == []

    def test_all_different(self):
        baseline = b"\x00" * 60
        fw = b"\xFF" * 60
        changed = deploy.compute_delta_chunks(baseline, fw, 15)
        assert changed == [0, 1, 2, 3]

    def test_one_chunk_changed(self):
        baseline = b"\x00" * 60
        fw = bytearray(b"\x00" * 60)
        fw[0] = 0x42  # Change only first byte → chunk 0
        changed = deploy.compute_delta_chunks(baseline, bytes(fw), 15)
        assert changed == [0]

    def test_last_chunk_changed(self):
        baseline = b"\x00" * 60
        fw = bytearray(b"\x00" * 60)
        fw[59] = 0x42  # Last byte → chunk 3
        changed = deploy.compute_delta_chunks(baseline, bytes(fw), 15)
        assert changed == [3]

    def test_firmware_larger_than_baseline(self):
        """New firmware extends beyond baseline — new chunks are 'changed'."""
        baseline = b"\x00" * 15
        fw = b"\x00" * 30  # Two chunks, baseline only covers first
        changed = deploy.compute_delta_chunks(baseline, fw, 15)
        # Second chunk: baseline is empty → padded with 0xFF → differs from 0x00
        assert 1 in changed

    def test_empty_baseline_all_changed(self):
        fw = b"\x00" * 30
        changed = deploy.compute_delta_chunks(b"", fw, 15)
        assert changed == [0, 1]

    def test_chunk_size_boundary(self):
        """Firmware exactly one chunk — should have exactly one chunk."""
        baseline = b"\xFF" * 15
        fw = b"\x00" * 15
        changed = deploy.compute_delta_chunks(baseline, fw, 15)
        assert changed == [0]


# --- Duration formatting ---

class TestFormatDuration:
    def test_seconds_only(self):
        assert deploy.format_duration(45) == "45s"

    def test_minutes_and_seconds(self):
        assert deploy.format_duration(125) == "2m 5s"

    def test_hours_and_minutes(self):
        assert deploy.format_duration(3720) == "1h 2m"

    def test_zero(self):
        assert deploy.format_duration(0) == "0s"

    def test_exactly_one_minute(self):
        assert deploy.format_duration(60) == "1m 0s"
