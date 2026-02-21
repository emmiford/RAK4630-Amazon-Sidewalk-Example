#!/usr/bin/env python3
"""
Firmware CLI — build, release, flash, and deploy firmware.

Usage:
    python aws/firmware.py release --version N    # patch, commit, tag, build
    python aws/firmware.py flash [app|all]        # USB flash via pyocd
    python aws/firmware.py deploy --version N     # OTA upload + monitor
    python aws/firmware.py baseline               # capture device state to S3
    python aws/firmware.py status [--watch [N]]   # OTA progress
    python aws/firmware.py abort                  # cancel OTA + clear session
    python aws/firmware.py clear-session          # clear DynamoDB session only
    python aws/firmware.py keygen [--force]       # generate signing keypair
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))

import ota
import release
from protocol_constants import crc32


# --- Subcommand handlers ---


def cmd_release(args):
    """Patch VERSION, build app, commit, and tag."""
    if args.version is None:
        print("ERROR: --version N is required (build version number, >= 1)")
        sys.exit(1)
    if args.version < 1:
        print("ERROR: --version must be >= 1 (0 is reserved for dev builds)")
        sys.exit(1)

    tag = f"app-v{args.version}"

    release.git_check_clean()
    release.git_check_tag(tag)
    release.set_app_version(args.version)
    release.build_app()
    release.git_commit_and_tag(args.version)

    print(f"\nRelease {tag} ready. Deploy with:")
    print(f"  python aws/firmware.py deploy --version {args.version}")


def cmd_flash(args):
    """Flash firmware via pyocd."""
    ver = release.get_app_version()
    if ver == 0:
        print("WARNING: VERSION is 0 (dev build). Use 'firmware release' for tagged builds.")

    target = args.target if args.target else "app"
    if target == "all":
        release.flash_all()
    elif target == "app":
        release.flash_app()
    else:
        print(f"ERROR: Unknown flash target '{target}'. Use 'app' or 'all'.")
        sys.exit(1)


def cmd_deploy(args):
    """OTA deploy: verify baseline, preview delta, sign, upload, monitor."""
    if args.version is None:
        print("ERROR: --version N is required")
        sys.exit(1)
    if args.version < 1:
        print("ERROR: --version must be >= 1")
        sys.exit(1)

    # Verify the tag exists (should have been created by 'release')
    tag = f"app-v{args.version}"

    # Load firmware binary
    abs_bin = os.path.join("/Users/emilyf/sidewalk-projects", ota.APP_BIN)
    if not os.path.exists(abs_bin):
        print(f"ERROR: {abs_bin} not found. Run 'firmware release --version {args.version}' first.")
        sys.exit(1)

    with open(abs_bin, "rb") as f:
        firmware = f.read()
    fw_crc = crc32(firmware)
    print(f"Firmware: {len(firmware)} bytes, CRC32=0x{fw_crc:08x}, build=v{args.version}")

    # Check for existing session
    session = ota.get_session()
    if session and not args.force:
        print(f"\nWARNING: Active OTA session exists (status: {session.get('status')})")
        print("Use --force to override, or 'abort'/'clear-session' first.")
        sys.exit(1)
    elif session and args.force:
        print("Clearing existing session (--force)")
        ota.clear_session()

    # Download S3 baseline and verify
    print("\nDownloading S3 baseline...")
    try:
        baseline = ota.s3_download(ota.BASELINE_KEY)
        bl_crc = crc32(baseline)
        print(f"S3 baseline: {len(baseline)} bytes, CRC32=0x{bl_crc:08x}")
    except Exception as e:
        print(f"No S3 baseline found ({e})")
        print("Run 'firmware baseline' first to capture device state.")
        sys.exit(1)

    if not args.remote:
        print("\nVerifying S3 baseline matches device primary...")
        device_data = ota.pyocd_read_primary()
        device_crc = crc32(device_data)
        if device_crc != bl_crc or len(device_data) != len(baseline):
            print("ERROR: Baseline mismatch!")
            print(f"  S3:     {len(baseline)} bytes, CRC32=0x{bl_crc:08x}")
            print(f"  Device: {len(device_data)} bytes, CRC32=0x{device_crc:08x}")
            print("Run 'baseline' to re-capture, or use --remote to skip verification.")
            sys.exit(1)
        print(f"  Match confirmed: CRC32=0x{device_crc:08x}")
    else:
        print("Skipping device verification (--remote)")

    # Preview delta
    changed = ota.compute_delta_chunks(baseline, firmware, ota.CHUNK_DATA_SIZE)
    full_chunks = (len(firmware) + ota.CHUNK_DATA_SIZE - 1) // ota.CHUNK_DATA_SIZE
    est_time = len(changed) * 15

    print("\nDelta preview:")
    print(f"  Changed: {len(changed)}/{full_chunks} chunks")
    print(f"  Indices: {changed}")
    print(f"  Est. transfer: ~{ota.format_duration(est_time)}")

    if not changed:
        print("\nNo changes detected — firmware matches baseline.")
        sys.exit(0)

    # Sign and upload
    is_signed = False
    upload_data = firmware
    if not args.unsigned:
        try:
            from ota_signing import OTA_SIG_SIZE, load_private_key, sign_firmware

            private_key = load_private_key()
            upload_data = sign_firmware(firmware, private_key)
            is_signed = True
            print(
                f"\nSigned: {len(firmware)}B firmware + "
                f"{OTA_SIG_SIZE}B signature = {len(upload_data)}B"
            )
        except FileNotFoundError:
            print("\nWARNING: No signing key found. Run 'keygen' first.")
            print("Deploying unsigned (use --unsigned to suppress this warning).")
    else:
        print("\nSkipping signing (--unsigned)")

    s3_key = f"firmware/app-v{args.version}.bin"
    s3_metadata = {}
    if is_signed:
        s3_metadata["signed"] = "true"

    print(f"Uploading to s3://{ota.OTA_BUCKET}/{s3_key} ...")
    ota.s3_upload(s3_key, upload_data, metadata=s3_metadata if s3_metadata else None)
    suffix = " (signed)" if is_signed else ""
    print(f"Upload complete — Lambda triggered{suffix}")

    # Monitor
    print("\nMonitoring OTA progress (Ctrl-C to stop)...\n")
    session_seen = False
    try:
        while True:
            time.sleep(5)
            session = ota.get_session()
            print("\033[2J\033[H", end="")
            print("OTA Deploy Monitor (Ctrl-C to stop)\n")
            if session:
                session_seen = True
                status = session.get("status", "unknown")
                ota.print_status(session)
                if status in ("complete",):
                    print("\nOTA complete!")
                    break
            elif session_seen:
                print("Session cleared (aborted or completed).")
                break
            else:
                print("Waiting for session to start...")
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped. Use 'status' to check later.")


def cmd_baseline(args):
    """Dump device primary partition to S3 as baseline."""
    data = ota.pyocd_read_primary()
    if not data:
        print("ERROR: Primary partition is empty (all 0xFF)")
        sys.exit(1)
    data_crc = crc32(data)
    print(f"Primary: {len(data)} bytes, CRC32=0x{data_crc:08x}")

    ota.s3_upload(ota.BASELINE_KEY, data)
    print(f"Uploaded to s3://{ota.OTA_BUCKET}/{ota.BASELINE_KEY}")

    local_path = "/tmp/ota_baseline.bin"
    with open(local_path, "wb") as f:
        f.write(data)
    print(f"Local copy: {local_path}")


def cmd_status(args):
    """Show OTA session status."""
    if args.watch is not None:
        interval = args.watch if args.watch else 30
        try:
            while True:
                print("\033[2J\033[H", end="")
                print(f"OTA Status  (Ctrl-C to stop, polling every {interval}s)\n")
                session = ota.get_session()
                active = ota.print_status(session)
                if not active:
                    break
                time.sleep(interval)
        except KeyboardInterrupt:
            print()
    else:
        session = ota.get_session()
        ota.print_status(session)


def cmd_abort(args):
    """Send OTA_ABORT to device and clear session."""
    ota.send_abort()
    time.sleep(1)
    ota.clear_session()
    print("Session cleared.")


def cmd_clear_session(args):
    """Clear DynamoDB session without sending abort to device."""
    ota.clear_session()
    print("Session cleared.")


def cmd_keygen(args):
    """Generate ED25519 signing keypair for OTA firmware signing."""
    from ota_signing import (
        PRIVATE_KEY_PATH,
        PUBLIC_KEY_PATH,
        extract_public_key_bytes,
        generate_keypair,
    )

    if os.path.exists(PRIVATE_KEY_PATH) and not args.force:
        print(f"Key already exists: {PRIVATE_KEY_PATH}")
        print("Use --force to overwrite.")
        sys.exit(1)

    _, public_key = generate_keypair()
    raw_bytes = extract_public_key_bytes(public_key)

    print(f"Private key: {PRIVATE_KEY_PATH}")
    print(f"Public key:  {PUBLIC_KEY_PATH}")
    print("\nRaw public key (32 bytes) for ota_signing.c:")
    hex_bytes = ", ".join(f"0x{b:02x}" for b in raw_bytes)
    print(f"  {{{hex_bytes}}}")


# --- Main ---


def main():
    parser = argparse.ArgumentParser(
        description="Firmware CLI — build, release, flash, and deploy firmware"
    )
    sub = parser.add_subparsers(dest="command", required=True)

    # release
    p_release = sub.add_parser("release", help="Patch VERSION, build, commit, tag")
    p_release.add_argument(
        "--version", type=int, default=None,
        help="Build version number (required, >= 1)",
    )

    # flash
    p_flash = sub.add_parser("flash", help="Flash firmware via pyocd")
    p_flash.add_argument(
        "target", nargs="?", default="app",
        choices=["app", "all"],
        help="Flash target (default: app)",
    )

    # deploy
    p_deploy = sub.add_parser("deploy", help="OTA upload + monitor")
    p_deploy.add_argument(
        "--version", type=int, default=None,
        help="Build version number (required, >= 1)",
    )
    p_deploy.add_argument(
        "--remote", action="store_true",
        help="Skip pyOCD baseline verification (device not connected)",
    )
    p_deploy.add_argument(
        "--force", action="store_true",
        help="Override existing OTA session",
    )
    p_deploy.add_argument(
        "--unsigned", action="store_true",
        help="Skip ED25519 signing",
    )

    # baseline
    sub.add_parser("baseline", help="Dump device primary -> S3 baseline")

    # status
    p_status = sub.add_parser("status", help="Monitor OTA progress")
    p_status.add_argument(
        "--watch", nargs="?", const=30, type=int, metavar="SEC",
        help="Poll every N seconds (default 30)",
    )

    # abort
    sub.add_parser("abort", help="Send OTA_ABORT + clear session")

    # clear-session
    sub.add_parser("clear-session", help="Clear DynamoDB session only")

    # keygen
    p_keygen = sub.add_parser("keygen", help="Generate ED25519 signing keypair")
    p_keygen.add_argument(
        "--force", action="store_true",
        help="Overwrite existing keypair",
    )

    args = parser.parse_args()

    commands = {
        "release": cmd_release,
        "flash": cmd_flash,
        "deploy": cmd_deploy,
        "baseline": cmd_baseline,
        "status": cmd_status,
        "abort": cmd_abort,
        "clear-session": cmd_clear_session,
        "keygen": cmd_keygen,
    }
    commands[args.command](args)


if __name__ == "__main__":
    main()
