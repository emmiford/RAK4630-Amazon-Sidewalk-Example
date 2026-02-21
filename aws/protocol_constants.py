"""
Shared protocol constants for EVSE monitor AWS Lambda functions.

These must match the corresponding C definitions in the firmware.
Only constants used by multiple files belong here.
"""

import binascii

# --- OTA protocol (must match ota_update.h) ---

OTA_CMD_TYPE = 0x20

# Uplink subtypes
OTA_SUB_ACK = 0x80
OTA_SUB_COMPLETE = 0x81
OTA_SUB_STATUS = 0x82

# --- EVSE wire-format magic bytes (must match evse_payload.h) ---

EVSE_MAGIC = 0xE5
DIAG_MAGIC = 0xE6

# --- Time sync ---

EPOCH_OFFSET = 1767225600  # 2026-01-01T00:00:00Z as Unix timestamp


# --- CRC ---

def crc32(data):
    """Compute CRC32 (IEEE) matching Zephyr's crc32_ieee."""
    return binascii.crc32(data) & 0xFFFFFFFF
