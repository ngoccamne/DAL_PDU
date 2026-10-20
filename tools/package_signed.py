"""Add AVR bootloader metadata, advancing the editable build ID on success."""

import argparse
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
COUNTER = ROOT / "signed_build_id.txt"
ORIGINAL = ROOT / "release" / "DAL_PDU.hex"
SIGNED = ROOT / "release" / "DAL_PDU_signed.hex"
DEFAULT_BOOT_HOST = ROOT.parent / "Bootloader" / "AVR_Booyloader" / "AVRBootLoader" / "tools" / "boot_host.py"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-host", type=Path, default=DEFAULT_BOOT_HOST)
    args = parser.parse_args()
    if not ORIGINAL.is_file():
        raise SystemExit(f"Original firmware is missing: {ORIGINAL}")
    if not args.boot_host.is_file():
        raise SystemExit(f"AVR packager is missing: {args.boot_host}")
    try:
        previous = int(COUNTER.read_text(encoding="ascii").strip())
    except (OSError, ValueError) as error:
        raise SystemExit(f"Invalid build ID in {COUNTER}: {error}") from error
    if previous < 0 or previous >= 0xFFFFFFFF:
        raise SystemExit("Build ID must be between 0 and 4294967294")
    build_id = previous + 1
    temporary = SIGNED.with_suffix(".hex.tmp")
    try:
        subprocess.run([sys.executable, str(args.boot_host), "package", str(ORIGINAL),
                        str(temporary), "--build", str(build_id)], check=True)
        os.replace(temporary, SIGNED)
        COUNTER.write_text(f"{build_id}\n", encoding="ascii")
    finally:
        temporary.unlink(missing_ok=True)
    print(f"Signed firmware: {SIGNED} (build ID {build_id})")


if __name__ == "__main__":
    main()
