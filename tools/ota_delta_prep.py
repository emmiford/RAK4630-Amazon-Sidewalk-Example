#!/usr/bin/env python3
"""
OTA Delta Prep — compute chunk-level diff between old and new firmware,
write changed chunks to staging via pyOCD, and print shell commands.

Usage:
  python3 ota_delta_prep.py <old.bin> <new.bin> [--chunk-size 15] [--write]

Without --write: prints the diff and shell commands only.
With --write: also writes changed chunks to staging flash (0xD0000) via pyOCD.
"""

import argparse
import binascii
import struct
import subprocess
import sys

STAGING_ADDR = 0xD0000
PYOCD = "/Users/emilyf/sidewalk-env/bin/pyocd"


def crc32(data):
    return binascii.crc32(data) & 0xFFFFFFFF


def compute_diff(old_bin, new_bin, chunk_size):
    """Return list of (abs_chunk_idx, new_chunk_data) for changed chunks."""
    full_chunks = (len(new_bin) + chunk_size - 1) // chunk_size
    changed = []

    for i in range(full_chunks):
        offset = i * chunk_size
        new_chunk = new_bin[offset:offset + chunk_size]
        old_chunk = old_bin[offset:offset + chunk_size] if offset < len(old_bin) else b""

        # Pad shorter chunk for comparison
        if len(old_chunk) < len(new_chunk):
            old_chunk = old_chunk + b"\xff" * (len(new_chunk) - len(old_chunk))

        if new_chunk != old_chunk:
            changed.append((i, new_chunk))

    return changed, full_chunks


def main():
    parser = argparse.ArgumentParser(description="OTA delta prep tool")
    parser.add_argument("old_bin", help="Path to old (baseline) firmware binary")
    parser.add_argument("new_bin", help="Path to new firmware binary")
    parser.add_argument("--chunk-size", type=int, default=15, help="Chunk size in bytes (default 15)")
    parser.add_argument("--write", action="store_true", help="Write changed chunks to staging via pyOCD")
    parser.add_argument("--erase", action="store_true", help="Erase staging before writing")
    args = parser.parse_args()

    with open(args.old_bin, "rb") as f:
        old_data = f.read()
    with open(args.new_bin, "rb") as f:
        new_data = f.read()

    new_crc = crc32(new_data)
    changed, full_chunks = compute_diff(old_data, new_data, args.chunk_size)

    print(f"Old: {len(old_data)}B, New: {len(new_data)}B")
    print(f"Chunk size: {args.chunk_size}B, Full chunks: {full_chunks}")
    print(f"Changed chunks: {len(changed)}/{full_chunks} ({100*len(changed)/full_chunks:.1f}%)")
    print(f"New CRC32: 0x{new_crc:08x}")
    print()

    if not changed:
        print("No changes detected!")
        return

    print("Changed chunk indices:", [c[0] for c in changed])
    print()

    # Show byte-level diff for each changed chunk
    for idx, new_chunk in changed:
        offset = idx * args.chunk_size
        old_chunk = old_data[offset:offset + args.chunk_size] if offset < len(old_data) else b"\xff" * len(new_chunk)
        if len(old_chunk) < len(new_chunk):
            old_chunk = old_chunk + b"\xff" * (len(new_chunk) - len(old_chunk))
        print(f"  Chunk {idx} (offset 0x{offset:04x}):")
        print(f"    old: {old_chunk.hex()}")
        print(f"    new: {new_chunk.hex()}")
    print()

    if args.erase or args.write:
        # Erase staging area
        erase_size = ((len(new_data) + 4095) // 4096) * 4096
        print(f"Erasing staging: 0x{STAGING_ADDR:x}, {erase_size}B...")
        subprocess.run([PYOCD, "cmd", "-t", "nrf52840", "-c",
                        f"erase 0x{STAGING_ADDR:x} {erase_size}"],
                       check=True)

    if args.write:
        # Write each changed chunk to staging
        for idx, chunk_data in changed:
            addr = STAGING_ADDR + idx * args.chunk_size
            # Write via a temp file
            tmp = f"/tmp/ota_chunk_{idx}.bin"
            with open(tmp, "wb") as f:
                f.write(chunk_data)
            print(f"Writing chunk {idx} ({len(chunk_data)}B) to 0x{addr:x}...")
            subprocess.run([PYOCD, "flash", "-t", "nrf52840",
                            "-a", f"0x{addr:x}", "--no-reset", tmp],
                           check=True)
        print()
        print("Chunks written to staging. Now run on device:")

    # Print shell command
    indices_str = " ".join(str(c[0]) for c in changed)
    print(f"\n  sid ota delta_test {args.chunk_size} {len(changed)} "
          f"{len(new_data)} 0x{new_crc:08x} {indices_str}")


if __name__ == "__main__":
    main()
