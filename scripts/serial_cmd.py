#!/usr/bin/env python3
"""Send a command to a serial port and capture the response."""
import sys
import time
import serial

def serial_command(port, command, baudrate=115200, timeout=3):
    try:
        ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout,
            write_timeout=timeout,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False
        )
    except serial.SerialException as e:
        print(f"Error opening port: {e}", file=sys.stderr)
        return None

    try:
        ser.reset_input_buffer()
        time.sleep(0.2)

        ser.write(f"\r\n{command}\r\n".encode('utf-8'))
        ser.flush()

        lines = []
        while True:
            line = ser.readline()
            if not line:
                break
            lines.append(line.decode('utf-8', errors='replace'))

        return ''.join(lines)
    finally:
        ser.close()

if __name__ == '__main__':
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbmodem101'
    cmd = sys.argv[2] if len(sys.argv) > 2 else 'sid status'
    result = serial_command(port, cmd)
    if result is not None:
        print(result, end='')
