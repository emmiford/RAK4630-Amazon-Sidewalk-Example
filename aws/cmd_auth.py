"""
Command authentication for charge control downlinks.

Signs payloads with truncated HMAC-SHA256 (8 bytes) before sending to the
device. The device verifies the tag before executing any charge control
command. This prevents a compromised cloud from sending arbitrary commands.

Key provisioning:
  - Cloud: CMD_AUTH_KEY environment variable (hex-encoded, 32 bytes)
  - Device: Compiled into cmd_auth.c (same 32-byte key)
  - Generate: python3 -c "import secrets; print(secrets.token_hex(32))"
"""

import hashlib
import hmac
import os

CMD_AUTH_TAG_SIZE = 8  # truncated HMAC output (bytes)
CMD_AUTH_KEY_SIZE = 32  # HMAC key length (bytes)


def get_auth_key():
    """Load the command auth HMAC key from environment.

    Returns bytes (32 bytes) or None if not configured.
    """
    key_hex = os.environ.get("CMD_AUTH_KEY", "")
    if not key_hex:
        return None
    key = bytes.fromhex(key_hex)
    if len(key) != CMD_AUTH_KEY_SIZE:
        raise ValueError(
            f"CMD_AUTH_KEY must be {CMD_AUTH_KEY_SIZE} bytes, got {len(key)}"
        )
    return key


def sign_command(payload, key):
    """Compute truncated HMAC-SHA256 tag for a command payload.

    Args:
        payload: Command payload bytes (before the tag).
        key: 32-byte HMAC key.

    Returns:
        8-byte authentication tag.
    """
    h = hmac.new(key, payload, hashlib.sha256)
    return h.digest()[:CMD_AUTH_TAG_SIZE]
