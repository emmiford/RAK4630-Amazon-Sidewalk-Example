"""
Release management — version patching, git tagging, and build invocation.

Used by firmware.py CLI. Can also be imported directly.
"""

import os
import subprocess
import sys

# --- Paths ---
REPO_ROOT = "/Users/emilyf/sidewalk-projects/rak-sid"
VERSION_PATH = os.path.join(REPO_ROOT, "app/rak4631_evse_monitor/VERSION")
PLATFORM_VERSION_PATH = os.path.join(REPO_ROOT, "app/rak4631_evse_monitor/PLATFORM_VERSION")
PYOCD = "/Users/emilyf/sidewalk-env/bin/pyocd"
BUILD_APP_DIR = "build_app"
APP_BIN = os.path.join(BUILD_APP_DIR, "app.bin")
APP_HEX = os.path.join(BUILD_APP_DIR, "app.hex")
FLASH_SCRIPT = os.path.join(REPO_ROOT, "app/rak4631_evse_monitor/flash.sh")

NRFUTIL_PREFIX = (
    "nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c"
)


# --- Version file helpers ---


def get_app_version():
    """Read app build version from VERSION file."""
    with open(VERSION_PATH, "r") as f:
        return int(f.read().strip())


def get_platform_version():
    """Read platform build version from PLATFORM_VERSION file."""
    with open(PLATFORM_VERSION_PATH, "r") as f:
        return int(f.read().strip())


def set_app_version(n):
    """Write app build version to VERSION file."""
    with open(VERSION_PATH, "w") as f:
        f.write(str(n))
    print(f"VERSION → {n}")


def set_platform_version(n):
    """Write platform build version to PLATFORM_VERSION file."""
    with open(PLATFORM_VERSION_PATH, "w") as f:
        f.write(str(n))
    print(f"PLATFORM_VERSION → {n}")


# --- Git safety helpers ---


def git_check_clean():
    """Ensure working tree is clean. Exit if dirty."""
    result = subprocess.run(
        ["git", "diff", "--quiet"], capture_output=True, cwd=REPO_ROOT,
    )
    staged = subprocess.run(
        ["git", "diff", "--cached", "--quiet"], capture_output=True,
        cwd=REPO_ROOT,
    )
    if result.returncode != 0 or staged.returncode != 0:
        print("ERROR: Working tree is dirty. Commit or stash changes first.")
        sys.exit(1)


def git_check_tag(tag):
    """Ensure tag does not already exist."""
    result = subprocess.run(
        ["git", "tag", "-l", tag], capture_output=True, text=True,
        cwd=REPO_ROOT,
    )
    if tag in result.stdout.strip().split("\n"):
        print(f"ERROR: Tag '{tag}' already exists. Choose a different version.")
        sys.exit(1)


def git_commit_and_tag(version, tag_prefix="app-v"):
    """Commit the VERSION file change and create an annotated git tag."""
    tag = f"{tag_prefix}{version}"

    subprocess.run(
        ["git", "add", VERSION_PATH], check=True, cwd=REPO_ROOT,
    )
    subprocess.run(
        ["git", "commit", "-m", f"Release {tag}"],
        check=True, cwd=REPO_ROOT,
    )
    subprocess.run(
        ["git", "tag", "-a", tag, "-m", f"App firmware build v{version}"],
        check=True, cwd=REPO_ROOT,
    )
    print(f"Committed and tagged: {tag}")


# --- Build helpers ---


def build_app():
    """Build the EVSE app via nrfutil toolchain-manager."""
    print("Building app...")
    build_cmd = (
        f"rm -rf {BUILD_APP_DIR} && mkdir {BUILD_APP_DIR} && "
        f"cd {BUILD_APP_DIR} && "
        f"cmake ../rak-sid/app/rak4631_evse_monitor/app_evse && make"
    )
    result = subprocess.run(
        f'{NRFUTIL_PREFIX} "{build_cmd}"',
        shell=True,
        cwd="/Users/emilyf/sidewalk-projects",
    )
    if result.returncode != 0:
        print("ERROR: Build failed")
        sys.exit(1)
    bin_path = os.path.join("/Users/emilyf/sidewalk-projects", APP_BIN)
    size = os.path.getsize(bin_path)
    print(f"Build OK: {bin_path} ({size} bytes)")
    return bin_path


def build_platform():
    """Build the platform image via west."""
    print("Building platform...")
    build_cmd = (
        "cd /Users/emilyf/sidewalk-projects && west build -p -b rak4631 "
        "rak-sid/app/rak4631_evse_monitor/ -- -DOVERLAY_CONFIG=lora.conf"
    )
    result = subprocess.run(
        f'{NRFUTIL_PREFIX} "{build_cmd}"',
        shell=True,
        cwd="/Users/emilyf/sidewalk-projects",
    )
    if result.returncode != 0:
        print("ERROR: Platform build failed")
        sys.exit(1)
    print("Platform build OK")


# --- Flash helpers ---


def flash_app():
    """Flash app image via pyocd (erase sector + write hex)."""
    hex_path = os.path.join("/Users/emilyf/sidewalk-projects", APP_HEX)
    if not os.path.exists(hex_path):
        print(f"ERROR: {hex_path} not found. Build first.")
        sys.exit(1)
    print("Erasing app partition (0x90000-0xCEFFF)...")
    subprocess.run(
        [PYOCD, "erase", "--target", "nrf52840", "--sector", "0x90000+0x3F000"],
        check=True,
    )
    print("Writing app hex...")
    subprocess.run(
        [PYOCD, "flash", "--target", "nrf52840", hex_path],
        check=True,
    )
    print("App flash complete")


def flash_all():
    """Flash all partitions via flash.sh."""
    subprocess.run(
        ["bash", FLASH_SCRIPT, "all"],
        check=True,
    )
    print("All images flashed")
