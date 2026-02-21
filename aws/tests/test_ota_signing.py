"""Tests for OTA firmware signing — ED25519 keygen, sign, verify, CLI integration."""

import os
import struct
import sys
import tempfile
from unittest.mock import MagicMock, patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


# --- Key generation and signing round-trips ---


class TestKeyGeneration:
    def test_generate_keypair_creates_files(self):
        from ota_signing import generate_keypair

        with tempfile.TemporaryDirectory() as tmpdir:
            key_path = os.path.join(tmpdir, "test.key")
            pub_path = os.path.join(tmpdir, "test.pub")
            with (
                patch("ota_signing.PRIVATE_KEY_PATH", key_path),
                patch("ota_signing.PUBLIC_KEY_PATH", pub_path),
                patch("ota_signing.KEY_DIR", tmpdir),
            ):
                private_key, public_key = generate_keypair()

            assert os.path.exists(key_path)
            assert os.path.exists(pub_path)
            # Private key file should be owner-only readable
            assert oct(os.stat(key_path).st_mode & 0o777) == "0o600"

    def test_round_trip_load_keys(self):
        from ota_signing import generate_keypair, load_private_key, load_public_key

        with tempfile.TemporaryDirectory() as tmpdir:
            key_path = os.path.join(tmpdir, "test.key")
            pub_path = os.path.join(tmpdir, "test.pub")
            with (
                patch("ota_signing.PRIVATE_KEY_PATH", key_path),
                patch("ota_signing.PUBLIC_KEY_PATH", pub_path),
                patch("ota_signing.KEY_DIR", tmpdir),
            ):
                generate_keypair()
                priv = load_private_key()
                pub = load_public_key()

            # Verify they form a valid pair
            msg = b"test message"
            sig = priv.sign(msg)
            pub.verify(sig, msg)  # Raises on failure

    def test_extract_public_key_bytes_is_32(self):
        from ota_signing import extract_public_key_bytes, generate_keypair

        with tempfile.TemporaryDirectory() as tmpdir:
            with (
                patch("ota_signing.PRIVATE_KEY_PATH", os.path.join(tmpdir, "k")),
                patch("ota_signing.PUBLIC_KEY_PATH", os.path.join(tmpdir, "p")),
                patch("ota_signing.KEY_DIR", tmpdir),
            ):
                _, pub = generate_keypair()

        raw = extract_public_key_bytes(pub)
        assert len(raw) == 32
        assert isinstance(raw, bytes)


class TestSignAndVerify:
    def _make_keys(self):
        from cryptography.hazmat.primitives.asymmetric.ed25519 import (
            Ed25519PrivateKey,
        )

        priv = Ed25519PrivateKey.generate()
        return priv, priv.public_key()

    def test_sign_appends_64_bytes(self):
        from ota_signing import OTA_SIG_SIZE, sign_firmware

        priv, _ = self._make_keys()
        firmware = b"\x00" * 100
        signed = sign_firmware(firmware, priv)
        assert len(signed) == len(firmware) + OTA_SIG_SIZE

    def test_sign_verify_round_trip(self):
        from ota_signing import sign_firmware, verify_signature

        priv, pub = self._make_keys()
        firmware = b"\xDE\xAD\xBE\xEF" * 256
        signed = sign_firmware(firmware, priv)
        assert verify_signature(signed, pub) is True

    def test_tampered_firmware_rejected(self):
        from ota_signing import sign_firmware, verify_signature

        priv, pub = self._make_keys()
        firmware = b"\x00" * 100
        signed = sign_firmware(firmware, priv)
        # Tamper with first byte
        tampered = bytes([signed[0] ^ 0xFF]) + signed[1:]
        assert verify_signature(tampered, pub) is False

    def test_tampered_signature_rejected(self):
        from ota_signing import sign_firmware, verify_signature

        priv, pub = self._make_keys()
        firmware = b"\x00" * 100
        signed = sign_firmware(firmware, priv)
        # Tamper with last byte of signature
        tampered = signed[:-1] + bytes([signed[-1] ^ 0xFF])
        assert verify_signature(tampered, pub) is False

    def test_wrong_key_rejected(self):
        from ota_signing import sign_firmware, verify_signature

        priv1, _ = self._make_keys()
        _, pub2 = self._make_keys()
        firmware = b"\x00" * 100
        signed = sign_firmware(firmware, priv1)
        assert verify_signature(signed, pub2) is False

    def test_empty_signed_bin_rejected(self):
        from ota_signing import verify_signature

        _, pub = self._make_keys()
        assert verify_signature(b"", pub) is False
        assert verify_signature(b"\x00" * 64, pub) is False  # Exactly sig size

    def test_ed25519_is_deterministic(self):
        from ota_signing import sign_firmware

        priv, _ = self._make_keys()
        firmware = b"\x42" * 50
        signed1 = sign_firmware(firmware, priv)
        signed2 = sign_firmware(firmware, priv)
        assert signed1 == signed2


# --- Deploy CLI integration ---


class TestDeploySigning:
    def test_s3_upload_accepts_metadata(self):
        """s3_upload passes Metadata kwarg when provided."""
        import ota
        import ota_deploy as deploy

        mock_s3 = MagicMock()
        with patch.object(ota, "get_s3", return_value=mock_s3):
            deploy.s3_upload("test/key", b"data", metadata={"signed": "true"})

        mock_s3.put_object.assert_called_once_with(
            Bucket=deploy.OTA_BUCKET,
            Key="test/key",
            Body=b"data",
            Metadata={"signed": "true"},
        )

    def test_s3_upload_no_metadata(self):
        """s3_upload works without metadata (backward compat)."""
        import ota
        import ota_deploy as deploy

        mock_s3 = MagicMock()
        with patch.object(ota, "get_s3", return_value=mock_s3):
            deploy.s3_upload("test/key", b"data")

        mock_s3.put_object.assert_called_once_with(
            Bucket=deploy.OTA_BUCKET,
            Key="test/key",
            Body=b"data",
        )


# --- Lambda flags byte ---


class TestLambdaFlagsByte:
    def test_build_ota_start_no_flags(self):
        """Without flags, OTA_START is 18 bytes (cmd+sub+payload)."""
        # Need to handle the import carefully since Lambda needs mocks
        if "sidewalk_utils" not in sys.modules:
            sys.modules["sidewalk_utils"] = MagicMock()
        if "boto3" not in sys.modules:
            sys.modules["boto3"] = MagicMock()

        import ota_sender_lambda as sender

        msg = sender.build_ota_start(
            total_size=1000,
            total_chunks=67,
            chunk_size=15,
            fw_crc32=0xDEADBEEF,
            version=3,
        )
        assert len(msg) == 18
        assert msg[0] == sender.OTA_CMD_TYPE
        assert msg[1] == sender.OTA_SUB_START

    def test_build_ota_start_with_signed_flag(self):
        """With flags=SIGNED, OTA_START is 19 bytes."""
        if "sidewalk_utils" not in sys.modules:
            sys.modules["sidewalk_utils"] = MagicMock()
        if "boto3" not in sys.modules:
            sys.modules["boto3"] = MagicMock()

        import ota_sender_lambda as sender

        msg = sender.build_ota_start(
            total_size=1000,
            total_chunks=67,
            chunk_size=15,
            fw_crc32=0xDEADBEEF,
            version=3,
            flags=sender.OTA_START_FLAGS_SIGNED,
        )
        assert len(msg) == 19
        assert msg[18] == sender.OTA_START_FLAGS_SIGNED

    def test_build_ota_start_unsigned_no_extra_byte(self):
        """flags=0 produces the same 18-byte message as no flags."""
        if "sidewalk_utils" not in sys.modules:
            sys.modules["sidewalk_utils"] = MagicMock()
        if "boto3" not in sys.modules:
            sys.modules["boto3"] = MagicMock()

        import ota_sender_lambda as sender

        msg_default = sender.build_ota_start(1000, 67, 15, 0xDEADBEEF, 3)
        msg_zero = sender.build_ota_start(1000, 67, 15, 0xDEADBEEF, 3, flags=0)
        assert msg_default == msg_zero
        assert len(msg_default) == 18

    def test_ota_start_payload_fields(self):
        """Verify the packed fields in OTA_START are correct."""
        if "sidewalk_utils" not in sys.modules:
            sys.modules["sidewalk_utils"] = MagicMock()
        if "boto3" not in sys.modules:
            sys.modules["boto3"] = MagicMock()

        import ota_sender_lambda as sender

        msg = sender.build_ota_start(
            total_size=4096,
            total_chunks=274,
            chunk_size=15,
            fw_crc32=0xAABBCCDD,
            version=5,
            flags=sender.OTA_START_FLAGS_SIGNED,
        )

        # Parse it back
        cmd, sub = msg[0], msg[1]
        total_size = struct.unpack_from("<I", msg, 2)[0]
        total_chunks = struct.unpack_from("<H", msg, 6)[0]
        chunk_size = struct.unpack_from("<H", msg, 8)[0]
        fw_crc32 = struct.unpack_from("<I", msg, 10)[0]
        version = struct.unpack_from("<I", msg, 14)[0]
        flags = msg[18]

        assert cmd == 0x20
        assert sub == 0x01
        assert total_size == 4096
        assert total_chunks == 274
        assert chunk_size == 15
        assert fw_crc32 == 0xAABBCCDD
        assert version == 5
        assert flags == 0x01
