#!/usr/bin/env python3
"""DEPRECATED: Use firmware.py instead. This wrapper redirects for backwards compatibility."""
import os
import sys

print("WARNING: ota_deploy.py is deprecated. Use firmware.py instead.", file=sys.stderr)

# Re-export pure functions for test compatibility
sys.path.insert(0, os.path.dirname(__file__))
# Forward CLI to firmware.py
import firmware  # noqa: E402
from ota import (  # noqa: E402, F401
    BUILD_APP_DIR,
    OTA_BUCKET,
    compute_delta_chunks,
    format_duration,
    get_s3,
    pyocd_dump,
    s3_upload,
)
from protocol_constants import crc32  # noqa: E402, F401


def main():
    firmware.main()


if __name__ == "__main__":
    main()
