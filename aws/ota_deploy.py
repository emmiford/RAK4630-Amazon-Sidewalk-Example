#!/usr/bin/env python3
"""DEPRECATED: Use firmware.py instead. This wrapper redirects for backwards compatibility."""
import sys
import os

print("WARNING: ota_deploy.py is deprecated. Use firmware.py instead.", file=sys.stderr)

# Re-export pure functions for test compatibility
sys.path.insert(0, os.path.dirname(__file__))
from ota import (  # noqa: F401
    BUILD_APP_DIR,
    compute_delta_chunks,
    format_duration,
    pyocd_dump,
)
from protocol_constants import crc32  # noqa: F401

# Forward CLI to firmware.py
import firmware  # noqa: E402


def main():
    firmware.main()


if __name__ == "__main__":
    main()
