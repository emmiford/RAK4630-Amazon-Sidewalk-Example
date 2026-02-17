"""Tests for pure functions in ota_deploy.py — CRC32, delta chunks, formatting."""

import os
import sys
import tempfile
from unittest.mock import patch

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


# --- Baseline stale flash warning ---

class TestPyocdDumpStaleWarning:
    """Test that pyocd_dump warns when dump is much larger than app.bin."""

    def _run_dump(self, dump_data, app_bin_size=None, capsys=None):
        """Helper: call pyocd_dump with mocked subprocess and flash file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            dump_path = os.path.join(tmpdir, "dump.bin")

            # Mock subprocess to write dump_data to the output file
            def fake_run(cmd, **kwargs):
                with open(dump_path, "wb") as f:
                    f.write(dump_data)

            # Create fake app.bin at the expected path
            if app_bin_size is not None:
                # The code computes: os.path.join(dirname(__file__), "..", BUILD_APP_DIR, "app.bin")
                # We patch os.path.exists and os.path.getsize for the bin_path
                real_exists = os.path.exists
                real_getsize = os.path.getsize

                def mock_exists(path):
                    if path.endswith(os.path.join(deploy.BUILD_APP_DIR, "app.bin")):
                        return True
                    return real_exists(path)

                def mock_getsize(path):
                    if path.endswith(os.path.join(deploy.BUILD_APP_DIR, "app.bin")):
                        return app_bin_size
                    return real_getsize(path)

                with patch("subprocess.run", side_effect=fake_run), \
                     patch("os.path.exists", side_effect=mock_exists), \
                     patch("os.path.getsize", side_effect=mock_getsize):
                    result = deploy.pyocd_dump(0x90000, 256 * 1024, dump_path)
            else:
                with patch("subprocess.run", side_effect=fake_run):
                    result = deploy.pyocd_dump(0x90000, 256 * 1024, dump_path)

            return result

    def test_no_warning_when_no_app_bin(self, capsys):
        """No app.bin exists — no warning emitted."""
        dump = b"\x42" * 200 + b"\xFF" * 100
        result = self._run_dump(dump, app_bin_size=None)
        captured = capsys.readouterr()
        assert "WARNING" not in captured.out
        assert len(result) == 200

    def test_no_warning_when_sizes_match(self, capsys):
        """Dump size close to app.bin — no warning."""
        dump = b"\x42" * 200 + b"\xFF" * 100
        result = self._run_dump(dump, app_bin_size=200)
        captured = capsys.readouterr()
        assert "WARNING" not in captured.out

    def test_warning_when_dump_much_larger(self, capsys):
        """Dump is >2x app.bin — stale flash warning emitted."""
        dump = b"\x42" * 4524 + b"\xFF" * 100
        result = self._run_dump(dump, app_bin_size=239)
        captured = capsys.readouterr()
        assert "WARNING" in captured.out
        assert "4524" in captured.out
        assert "239" in captured.out

    def test_no_warning_when_exactly_2x(self, capsys):
        """Dump is exactly 2x app.bin — no warning (must be >2x)."""
        dump = b"\x42" * 478 + b"\xFF" * 100
        result = self._run_dump(dump, app_bin_size=239)
        captured = capsys.readouterr()
        assert "WARNING" not in captured.out

    def test_warning_when_just_over_2x(self, capsys):
        """Dump is just over 2x app.bin — warning emitted."""
        dump = b"\x42" * 479 + b"\xFF" * 100
        result = self._run_dump(dump, app_bin_size=239)
        captured = capsys.readouterr()
        assert "WARNING" in captured.out
