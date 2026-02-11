"""pytest fixtures for serial shell integration tests."""

import time

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--serial-port",
        default="/dev/tty.usbmodem101",
        help="Serial port for device under test",
    )
    parser.addoption(
        "--baudrate",
        default=115200,
        type=int,
        help="Serial port baud rate",
    )


@pytest.fixture(scope="session")
def serial_port(request):
    return request.config.getoption("--serial-port")


@pytest.fixture(scope="session")
def baudrate(request):
    return request.config.getoption("--baudrate")


@pytest.fixture(scope="session")
def device(serial_port, baudrate):
    """Provide a serial device connection for the test session."""
    try:
        import serial
    except ImportError:
        pytest.skip("pyserial not installed (pip install pyserial)")

    try:
        ser = serial.Serial(
            port=serial_port,
            baudrate=baudrate,
            timeout=3,
            write_timeout=3,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        )
    except serial.SerialException as e:
        pytest.skip(f"Cannot open serial port {serial_port}: {e}")

    yield ser
    ser.close()


def send_and_expect(device, cmd, expected_substring, timeout=5):
    """Send a shell command and verify the response contains expected text.

    Returns the full response string.
    Raises AssertionError if expected_substring is not found.
    """
    device.reset_input_buffer()
    time.sleep(0.2)
    device.write(f"\r\n{cmd}\r\n".encode("utf-8"))
    device.flush()

    deadline = time.time() + timeout
    lines = []
    while time.time() < deadline:
        line = device.readline()
        if not line:
            break
        lines.append(line.decode("utf-8", errors="replace"))

    response = "".join(lines)
    assert expected_substring in response, (
        f"Expected '{expected_substring}' in response to '{cmd}'.\n"
        f"Got: {response[:500]}"
    )
    return response
