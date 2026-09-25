#!/usr/bin/env python3
"""Package the VQEAF OS build into flashable .img files.

Reads the PlatformIO build output and merges bootloader, partition table,
OTA selector and application into single images that can be written to the
chip starting at offset 0x0.

Outputs (into --out, default ./dist):
  <name>_merged.img  sparse image, flash at 0x0. Compact.
  <name>_16mb.img    identical content padded with 0xFF to the full 16 MiB,
                     for tools that expect a whole-chip image.

Usage:
    py -3 tools/make_flash_image.py
    py -3 tools/make_flash_image.py --env vqeaf_os --out dist --name vqeaf_os_v2.4.0

Requires: a completed `pio run -e <env>` and esptool (from PlatformIO's
tool-esptoolpy package, or `esptool` on PATH).
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent

# Flash layout for partitions/vqeaf_16mb_ota.csv, confirmed against the
# `pio run -t upload` log (esptool writes exactly these four offsets).
LAYOUT = (
    (0x0000, "bootloader.bin"),
    (0x8000, "partitions.bin"),
    (0xE000, "boot_app0.bin"),
    (0x10000, "firmware.bin"),
)

# Structural markers used to verify the merged image.
ESP_IMAGE_MAGIC = 0xE9
PARTITION_TABLE_MAGIC = b"\xaa\x50"

FLASH_SIZE_BYTES = 16 * 1024 * 1024


def read_ini_flash_settings(env: str) -> dict:
    """Pull flash mode/freq/size out of platformio.ini so the image cannot drift."""
    ini = PROJECT_ROOT / "platformio.ini"
    text = ini.read_text(encoding="utf-8") if ini.exists() else ""

    mode = "qio"
    freq = "80m"
    size = "16MB"

    m = re.search(r"^\s*board_build\.flash_mode\s*=\s*(\S+)", text, re.M)
    if m:
        mode = m.group(1)
    m = re.search(r"^\s*board_build\.f_flash\s*=\s*(\d+)", text, re.M)
    if m:
        hz = int(m.group(1))
        freq = {80_000_000: "80m", 40_000_000: "40m"}.get(hz, "80m")
    m = re.search(r"^\s*board_upload\.flash_size\s*=\s*(\S+)", text, re.M)
    if m:
        size = m.group(1)

    if mode == "qio" and size == "16MB":
        pass  # matches the N16R8 board definition
    return {"mode": mode, "freq": freq, "size": size}


def find_esptool() -> list:
    """Return the argv prefix that runs esptool."""
    pkg = Path.home() / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"
    if pkg.exists():
        return [sys.executable, str(pkg)]
    exe = shutil.which("esptool") or shutil.which("esptool.py")
    if exe:
        return [exe]
    raise SystemExit(
        "esptool not found. Install PlatformIO (its tool-esptoolpy package is "
        "used automatically) or put esptool on PATH."
    )


def find_boot_app0() -> Path:
    """boot_app0.bin ships with the Arduino-ESP32 framework, not the build dir."""
    fw_root = Path.home() / ".platformio" / "packages" / "framework-arduinoespressif32"
    candidate = fw_root / "tools" / "partitions" / "boot_app0.bin"
    if candidate.exists():
        return candidate
    for hit in fw_root.glob("**/boot_app0.bin"):
        return hit
    raise SystemExit(f"boot_app0.bin not found under {fw_root}")


def collect_inputs(env: str) -> list:
    """Resolve every (offset, path) pair, failing loudly if anything is missing."""
    build_dir = PROJECT_ROOT / ".pio" / "build" / env
    if not build_dir.is_dir():
        raise SystemExit(
            f"no build output at {build_dir}. Run: pio run -e {env}"
        )

    resolved = []
    for offset, name in LAYOUT:
        if name == "boot_app0.bin":
            path = find_boot_app0()
        else:
            path = build_dir / name
        if not path.exists():
            raise SystemExit(f"missing {name}: {path}")
        if path.stat().st_size == 0:
            raise SystemExit(f"{name} is empty: {path}")
        resolved.append((offset, path))
    return resolved


def run_merge(esptool: list, inputs: list, out: Path, flash: dict, fill: bool) -> None:
    cmd = esptool + [
        "--chip", "esp32s3",
        "merge_bin",
        "-o", str(out),
        "--flash_mode", flash["mode"],
        "--flash_freq", flash["freq"],
        "--flash_size", flash["size"],
    ]
    if fill:
        cmd += ["--fill-flash-size", flash["size"]]
    for offset, path in inputs:
        cmd += [hex(offset), str(path)]

    print("  $ " + " ".join(cmd))
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stdout)
        print(proc.stderr, file=sys.stderr)
        raise SystemExit(f"esptool merge_bin failed (exit {proc.returncode})")


def verify(img: Path, inputs: list, expect_full: bool) -> None:
    """Re-read the produced image and check the structure, not just that it exists."""
    data = img.read_bytes()
    problems = []

    for offset, path in inputs:
        if offset >= len(data):
            problems.append(f"offset {offset:#06x} beyond end of image")
            continue
        got = data[offset:offset + path.stat().st_size]
        want = path.read_bytes()
        if got != want:
            problems.append(f"content mismatch at {offset:#06x} ({path.name})")

    if data[0:1] != bytes([ESP_IMAGE_MAGIC]):
        problems.append(f"no ESP image magic at 0x0000 (got {data[0]:#04x})")
    if data[0x8000:0x8002] != PARTITION_TABLE_MAGIC:
        problems.append("no partition table magic 0xAA50 at 0x8000")
    if data[0x10000:0x10001] != bytes([ESP_IMAGE_MAGIC]):
        problems.append(f"no ESP image magic at 0x10000 (got {data[0x10000]:#04x})")

    if expect_full and len(data) != FLASH_SIZE_BYTES:
        problems.append(
            f"expected {FLASH_SIZE_BYTES} bytes for a full image, got {len(data)}"
        )

    if problems:
        for p in problems:
            print(f"  FAIL {p}")
        raise SystemExit("image verification failed")
    print(f"  OK  {len(data):,} bytes, all 4 regions byte-identical to the inputs")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--env", default="vqeaf_os", help="PlatformIO environment")
    ap.add_argument("--out", default="dist", help="output directory")
    ap.add_argument("--name", default="vqeaf_os_v2.4.0", help="base filename")
    args = ap.parse_args()

    out_dir = (PROJECT_ROOT / args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    flash = read_ini_flash_settings(args.env)
    esptool = find_esptool()
    inputs = collect_inputs(args.env)

    print(f"env={args.env}  flash={flash['mode']}/{flash['freq']}/{flash['size']}")
    print("inputs:")
    for offset, path in inputs:
        print(f"  {offset:#06x}  {path.stat().st_size:>9,}  {path.name}")

    merged = out_dir / f"{args.name}_merged.img"
    full = out_dir / f"{args.name}_16mb.img"

    print("\n[1/2] sparse merged image (flash at 0x0)")
    run_merge(esptool, inputs, merged, flash, fill=False)
    verify(merged, inputs, expect_full=False)

    print("\n[2/2] full 16 MiB image")
    run_merge(esptool, inputs, full, flash, fill=True)
    verify(full, inputs, expect_full=True)

    print("\nwritten:")
    for p in (merged, full):
        print(f"  {p}  ({p.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
