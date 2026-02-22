"""
Shared protocol constants for EVSE monitor AWS Lambda functions.

These must match the corresponding C definitions in the firmware.
Only constants used by multiple files belong here.
"""

import binascii
from datetime import datetime, timezone, timedelta
from zoneinfo import ZoneInfo

# --- OTA protocol (must match ota_update.h) ---

OTA_CMD_TYPE = 0x20

# Uplink subtypes
OTA_SUB_ACK = 0x80
OTA_SUB_COMPLETE = 0x81
OTA_SUB_STATUS = 0x82

# --- EVSE wire-format magic bytes (must match evse_payload.h) ---

TELEMETRY_MAGIC = 0xE5
DIAG_MAGIC = 0xE6

# --- Time sync ---

EPOCH_OFFSET = 1767225600  # 2026-01-01T00:00:00Z as Unix timestamp

# --- Timezone ---

MT = ZoneInfo("America/Denver")  # DST-aware Mountain Time


def unix_ms_to_mt(unix_ms):
    """Convert Unix milliseconds to Mountain Time string for DynamoDB SK.

    Format: 'YYYY-MM-DD HH:MM:SS.mmm' (e.g., '2026-02-21 14:30:00.123')
    Uses America/Denver for DST-aware conversion.
    """
    dt = datetime.fromtimestamp(unix_ms / 1000, tz=MT)
    return dt.strftime("%Y-%m-%d %H:%M:%S") + f".{int(unix_ms) % 1000:03d}"


def now_mt():
    """Return current time as Mountain Time string for DynamoDB SK."""
    import time
    return unix_ms_to_mt(int(time.time() * 1000))


# --- CRC ---

def crc32(data):
    """Compute CRC32 (IEEE) matching Zephyr's crc32_ieee."""
    return binascii.crc32(data) & 0xFFFFFFFF
