"""Shared test fixtures for AWS Lambda tests."""

import os
import sys
from unittest.mock import MagicMock

# Ensure aws/ is on the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

# --- Module-level mocking for boto3 and sidewalk_utils ---
# Lambda modules import these at module scope. We mock them before import
# so the modules initialize cleanly in the test environment.

# Only install mocks if not already present (test_ota_sender.py does its own)
if "sidewalk_utils" not in sys.modules:
    mock_sidewalk_utils = MagicMock()
    mock_sidewalk_utils.get_device_id.return_value = "test-device-id"
    mock_sidewalk_utils.send_sidewalk_msg = MagicMock()
    sys.modules["sidewalk_utils"] = mock_sidewalk_utils

if "boto3" not in sys.modules:
    sys.modules["boto3"] = MagicMock()
