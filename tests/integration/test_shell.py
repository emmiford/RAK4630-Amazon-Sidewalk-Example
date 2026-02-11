"""
Serial shell integration tests — requires a flashed device connected via USB.

Usage:
    pytest tests/integration/test_shell.py -v --serial-port /dev/tty.usbmodem101
"""

import time

import pytest

from conftest import send_and_expect


class TestEvseShell:
    """EVSE shell command tests."""

    def test_evse_status_reports(self, device):
        """'evse status' should report J1772, charging, and thermostat info."""
        response = send_and_expect(device, "app evse status", "J1772")
        assert "Charging" in response or "charging" in response

    def test_simulate_state_a(self, device):
        """'evse a' should simulate J1772 state A."""
        send_and_expect(device, "app evse a", "Simulat")
        time.sleep(0.5)
        send_and_expect(device, "app evse status", "A")

    def test_simulate_state_c(self, device):
        """'evse c' should simulate J1772 state C."""
        send_and_expect(device, "app evse c", "Simulat")
        time.sleep(0.5)
        send_and_expect(device, "app evse status", "C")

    def test_simulation_expires(self, device):
        """Simulation should expire after ~10 seconds."""
        send_and_expect(device, "app evse c", "Simulat")
        time.sleep(11)
        response = send_and_expect(device, "app evse status", "J1772")
        # After expiry, state should be real reading (not necessarily C)
        assert "Simulat" not in response or "expired" in response.lower()

    def test_charge_pause_allow(self, device):
        """'evse pause' and 'evse allow' should toggle charge control."""
        send_and_expect(device, "app evse pause", "")
        time.sleep(0.5)
        response = send_and_expect(device, "app evse status", "NO")

        send_and_expect(device, "app evse allow", "")
        time.sleep(0.5)
        send_and_expect(device, "app evse status", "YES")


class TestSidewalkShell:
    """Sidewalk connectivity shell tests."""

    def test_sid_status(self, device):
        """'sid status' should report connectivity."""
        send_and_expect(device, "sid status", "Ready")

    def test_sid_send_when_ready(self, device):
        """'sid send' should queue or report not-ready."""
        response = send_and_expect(device, "app sid send", "")
        assert "queued" in response.lower() or "not ready" in response.lower() or \
               "sent" in response.lower() or "ready" in response.lower()
