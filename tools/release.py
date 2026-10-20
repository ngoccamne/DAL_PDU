"""Copies the firmware image to release/ with the version and build in its name.

Usage: python release.py [Debug|Release]

The image is DAL_PDU_v<MAJOR>.<MINOR>.<PATCH>_<build>.hex and .elf, where <build> is the short
commit hash, with "-dirty" when the tree had uncommitted changes. Run it after the build, as the
Studio post-build event. It reads the version and the build from the generated headers.
"""
import os
import re
import shutil
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
PDU = os.path.dirname(TOOLS)
PROJECT = os.path.join(PDU, "firmware", "DAL_PDU", "DAL_PDU")
VERSION_H = os.path.join(PROJECT, "src", "dev", "M1_Sys", "Version", "version.h")
BUILD_H = os.path.join(PROJECT, "src", "dev", "M1_Sys", "Version", "build_id.h")
RELEASE = os.path.join(PDU, "release")


def define_value(path, name):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    match = re.search(r"#define\s+%s\s+(\S+)" % name, text)
    if not match:
        raise SystemExit("%s not found in %s" % (name, path))
    return match.group(1).strip('"')


def main():
    config = sys.argv[1] if len(sys.argv) > 1 else "Debug"
    version = "%s.%s.%s" % (
        define_value(VERSION_H, "FIRMWARE_VERSION_MAJOR"),
        define_value(VERSION_H, "FIRMWARE_VERSION_MINOR"),
        define_value(VERSION_H, "FIRMWARE_VERSION_PATCH"),
    )
    build = define_value(BUILD_H, "FIRMWARE_BUILD_ID")
    source_dir = os.path.join(PROJECT, config)
    os.makedirs(RELEASE, exist_ok=True)
    copied = []
    for ext in (".hex", ".elf"):
        source = os.path.join(source_dir, "DAL_PDU" + ext)
        if not os.path.exists(source):
            raise SystemExit("missing build output: %s" % source)
        target = os.path.join(RELEASE, "DAL_PDU_v%s_%s%s" % (version, build, ext))
        shutil.copyfile(source, target)
        copied.append(target)
    for path in copied:
        print("release: %s" % os.path.relpath(path, PDU))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
