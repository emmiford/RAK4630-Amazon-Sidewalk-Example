"""
OTA Firmware Signing — ED25519 sign/verify for app OTA images.

Signs firmware binaries by appending a 64-byte ED25519 signature.
The device verifies the signature before applying the update.

Key storage:
    Private key: ~/.sidecharge/ota_signing.key (PEM)
    Public key:  ~/.sidecharge/ota_signing.pub (PEM)
"""

import os

from cryptography.hazmat.primitives.asymmetric.ed25519 import (
    Ed25519PrivateKey,
    Ed25519PublicKey,
)
from cryptography.hazmat.primitives.serialization import (
    Encoding,
    NoEncryption,
    PrivateFormat,
    PublicFormat,
)

OTA_SIG_SIZE = 64
KEY_DIR = os.path.expanduser("~/.sidecharge")
PRIVATE_KEY_PATH = os.path.join(KEY_DIR, "ota_signing.key")
PUBLIC_KEY_PATH = os.path.join(KEY_DIR, "ota_signing.pub")


def generate_keypair():
    """Generate an ED25519 keypair and save to ~/.sidecharge/."""
    os.makedirs(KEY_DIR, exist_ok=True)

    private_key = Ed25519PrivateKey.generate()

    # Save private key (PEM)
    pem_private = private_key.private_bytes(
        Encoding.PEM, PrivateFormat.PKCS8, NoEncryption()
    )
    with open(PRIVATE_KEY_PATH, "wb") as f:
        f.write(pem_private)
    os.chmod(PRIVATE_KEY_PATH, 0o600)

    # Save public key (PEM)
    public_key = private_key.public_key()
    pem_public = public_key.public_bytes(Encoding.PEM, PublicFormat.SubjectPublicKeyInfo)
    with open(PUBLIC_KEY_PATH, "wb") as f:
        f.write(pem_public)

    return private_key, public_key


def load_private_key():
    """Load the ED25519 private key from ~/.sidecharge/."""
    from cryptography.hazmat.primitives.serialization import load_pem_private_key

    with open(PRIVATE_KEY_PATH, "rb") as f:
        return load_pem_private_key(f.read(), password=None)


def load_public_key():
    """Load the ED25519 public key from ~/.sidecharge/."""
    from cryptography.hazmat.primitives.serialization import load_pem_public_key

    with open(PUBLIC_KEY_PATH, "rb") as f:
        return load_pem_public_key(f.read())


def sign_firmware(firmware, private_key):
    """Sign firmware and return firmware + 64-byte ED25519 signature appended."""
    signature = private_key.sign(firmware)
    assert len(signature) == OTA_SIG_SIZE
    return firmware + signature


def verify_signature(signed_bin, public_key):
    """Verify an ED25519-signed firmware binary. Returns True if valid."""
    if len(signed_bin) <= OTA_SIG_SIZE:
        return False
    firmware = signed_bin[:-OTA_SIG_SIZE]
    signature = signed_bin[-OTA_SIG_SIZE:]
    try:
        public_key.verify(signature, firmware)
        return True
    except Exception:
        return False


def extract_public_key_bytes(public_key=None):
    """Extract the raw 32-byte ED25519 public key for embedding in C firmware."""
    if public_key is None:
        public_key = load_public_key()
    raw = public_key.public_bytes(Encoding.Raw, PublicFormat.Raw)
    assert len(raw) == 32
    return raw
