#!/usr/bin/env python3
"""Serial automation for live EVSE data generation via Sidewalk.

Connects to the device serial port, cycles through J1772 simulation states,
and triggers real uplinks.  Each uplink travels:
  device → LoRa → Sidewalk → IoT Rule → decode Lambda → DynamoDB

Requires: device powered on, Sidewalk-registered, LoRa connected.
Install:  pip install pyserial

Usage:
    python3 aws/serial_data_gen.py                      # Default cycle
    python3 aws/serial_data_gen.py --cycles 5            # 5 state cycles
    python3 aws/serial_data_gen.py --port /dev/tty.usbmodem101
    python3 aws/serial_data_gen.py --dry-run             # Print commands only
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed.  pip install pyserial")
    sys.exit(1)

DEFAULT_PORT = "/dev/tty.usbmodem101"
BAUD_RATE = 115200
UPLINK_INTERVAL_S = 6  # >5s rate limit

# Shell commands matching app_entry.c shell handlers
CMD_STATE_A = "app evse a"
CMD_STATE_B = "app evse b"
CMD_STATE_C = "app evse c"
CMD_SEND = "app sid send"
CMD_ALLOW = "app evse allow"
CMD_PAUSE = "app evse pause"
CMD_STATUS = "app evse status"


def send_cmd(ser, cmd, wait_s=1.0, dry_run=False):
    """Send a shell command and read response."""
    full_cmd = cmd + "\r\n"
    if dry_run:
        print(f"  [DRY] > {cmd}")
        time.sleep(0.1)
        return ""

    ser.write(full_cmd.encode())
    time.sleep(wait_s)
    response = ""
    while ser.in_waiting:
        response += ser.read(ser.in_waiting).decode("utf-8", errors="replace")
    return response


def send_and_uplink(ser, state_cmd, label, dry_run=False):
    """Set a simulation state and trigger an uplink."""
    print(f"\n--- {label} ---")
    resp = send_cmd(ser, state_cmd, wait_s=1.0, dry_run=dry_run)
    if resp:
        print(f"  State response: {resp.strip()[:120]}")

    time.sleep(1)

    print(f"  Sending uplink...")
    resp = send_cmd(ser, CMD_SEND, wait_s=2.0, dry_run=dry_run)
    if resp:
        # Look for confirmation in output
        for line in resp.strip().split("\n"):
            if "send" in line.lower() or "uplink" in line.lower() or "sid" in line.lower():
                print(f"  {line.strip()}")

    # Wait for rate limit
    print(f"  Waiting {UPLINK_INTERVAL_S}s (rate limit)...")
    if not dry_run:
        time.sleep(UPLINK_INTERVAL_S)
    else:
        time.sleep(0.2)


def run_cycle(ser, cycle_num, dry_run=False):
    """Run one full A→B→C→A cycle with uplinks at each state."""
    print(f"\n{'='*50}")
    print(f"CYCLE {cycle_num}")
    print(f"{'='*50}")

    send_and_uplink(ser, CMD_STATE_A, "State A: No vehicle", dry_run=dry_run)
    send_and_uplink(ser, CMD_STATE_B, "State B: Vehicle connected", dry_run=dry_run)
    send_and_uplink(ser, CMD_STATE_C, "State C: Charging", dry_run=dry_run)

    # Extra C-state uplinks to simulate active charging
    for i in range(2):
        print(f"\n--- State C: Charging (heartbeat {i+1}) ---")
        resp = send_cmd(ser, CMD_SEND, wait_s=2.0, dry_run=dry_run)
        if resp:
            for line in resp.strip().split("\n"):
                if "send" in line.lower() or "uplink" in line.lower():
                    print(f"  {line.strip()}")
        if not dry_run:
            time.sleep(UPLINK_INTERVAL_S)
        else:
            time.sleep(0.2)

    send_and_uplink(ser, CMD_STATE_A, "State A: Charge complete", dry_run=dry_run)


def run_charge_control_test(ser, dry_run=False):
    """Test charge allow/pause commands with uplinks."""
    print(f"\n{'='*50}")
    print("CHARGE CONTROL TEST")
    print(f"{'='*50}")

    send_and_uplink(ser, CMD_STATE_C, "State C: Charging", dry_run=dry_run)

    print("\n--- Pausing charge ---")
    send_cmd(ser, CMD_PAUSE, wait_s=1.0, dry_run=dry_run)
    send_and_uplink(ser, CMD_STATE_C, "State C: Paused", dry_run=dry_run)

    print("\n--- Allowing charge ---")
    send_cmd(ser, CMD_ALLOW, wait_s=1.0, dry_run=dry_run)
    send_and_uplink(ser, CMD_STATE_C, "State C: Allowed", dry_run=dry_run)

    send_and_uplink(ser, CMD_STATE_A, "State A: Done", dry_run=dry_run)


def main():
    parser = argparse.ArgumentParser(
        description="Generate live EVSE data via serial + Sidewalk")
    parser.add_argument("--port", default=DEFAULT_PORT,
                        help=f"Serial port (default: {DEFAULT_PORT})")
    parser.add_argument("--cycles", type=int, default=3,
                        help="Number of A→B→C→A cycles (default: 3)")
    parser.add_argument("--charge-test", action="store_true",
                        help="Include charge allow/pause test")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print commands without sending")
    args = parser.parse_args()

    ser = None
    if not args.dry_run:
        print(f"Connecting to {args.port} at {BAUD_RATE} baud...")
        try:
            ser = serial.Serial(args.port, BAUD_RATE, timeout=2)
            time.sleep(1)  # Let device settle
            # Drain any pending output
            while ser.in_waiting:
                ser.read(ser.in_waiting)
        except serial.SerialException as e:
            print(f"ERROR: Cannot open {args.port}: {e}")
            sys.exit(1)
        print("Connected.\n")

        # Check device status first
        print("--- Device Status ---")
        resp = send_cmd(ser, CMD_STATUS, wait_s=2.0)
        if resp:
            print(resp.strip())
    else:
        print("[DRY RUN MODE]\n")

    try:
        for cycle in range(1, args.cycles + 1):
            run_cycle(ser, cycle, dry_run=args.dry_run)

        if args.charge_test:
            run_charge_control_test(ser, dry_run=args.dry_run)

        # Final status
        if not args.dry_run:
            print(f"\n{'='*50}")
            print("FINAL STATUS")
            print(f"{'='*50}")
            resp = send_cmd(ser, CMD_STATUS, wait_s=2.0)
            if resp:
                print(resp.strip())

        total_uplinks = args.cycles * 6 + (4 if args.charge_test else 0)
        print(f"\nDone. ~{total_uplinks} uplinks sent over "
              f"~{total_uplinks * UPLINK_INTERVAL_S}s")

    except KeyboardInterrupt:
        print("\n\nInterrupted by user.")
    finally:
        if ser and not args.dry_run:
            # Return to State A before disconnecting
            send_cmd(ser, CMD_STATE_A, wait_s=1.0)
            ser.close()
            print("Serial port closed.")


if __name__ == "__main__":
    main()
