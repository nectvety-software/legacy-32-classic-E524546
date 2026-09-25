#!/usr/bin/env python3
"""Generate the images embedded in docs/guide/HUONG_DAN_NAP_FIRMWARE.md.

Everything drawn here comes from the real captured logs archived in
docs/guide/logs/ - no output is invented. Re-run after a new flash to refresh
the guide:

    py -3 tools/make_flash_guide_images.py

Requires Pillow.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import render_terminal_png as rtp  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
LOGS = ROOT / "docs" / "guide" / "logs"
OUT = ROOT / "docs" / "guide" / "images"

SEGOE = "C:/Windows/Fonts/segoeui.ttf"
CONSOLA = "C:/Windows/Fonts/consola.ttf"
CONSOLA_B = "C:/Windows/Fonts/consolab.ttf"

INK = "#1B1B1B"
MUTED = "#5A6472"
PANEL = "#FFFFFF"
FRAME = "#D7DCE3"

# Palette for the layout diagram.
C_BOOT = "#2F7FD1"
C_PART = "#D19A2F"
C_NVS = "#9B59B6"
C_OTA0 = "#2E9E4F"
C_EMPTY = "#DDE2E8"
C_SPIFFS = "#B9C2CC"
C_CORE = "#C9D0D8"


def font(path: str, size: int):
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.truetype(SEGOE, size)


# --------------------------------------------------------------------------
# excerpt building - selects verbatim lines out of the archived logs
# --------------------------------------------------------------------------

def pick(lines, patterns, keep_first=0):
    """Return lines matching any pattern, preserving order.

    keep_first limits how many consecutive matches of the same pattern to keep
    (used to collapse hundreds of `Compiling` lines into two).
    """
    out, seen = [], {}
    for ln in lines:
        for pat in patterns:
            if re.search(pat, ln):
                seen[pat] = seen.get(pat, 0)
                if keep_first and seen[pat] >= keep_first:
                    seen[pat] += 1
                    break
                seen[pat] += 1
                out.append(ln)
                break
    return out


def build_build_excerpt():
    lines = (LOGS / "build.log").read_text(encoding="utf-8").splitlines()
    head = pick(lines, [
        r"^Processing ", r"^CONFIGURATION:", r"^PLATFORM:", r"^HARDWARE:",
        r"^LDF Modes:", r"^Found \d+ compatible", r"^Building in release mode$",
    ])
    comp = pick(lines, [r"^Compiling "], keep_first=2)
    tail = pick(lines, [
        r"^Building \\?\.pio", r"^Linking ", r"^Checking size",
        r"^RAM:", r"^Flash:", r"^Successfully created",
        r"\[SUCCESS\]",
    ])
    n_comp = sum(1 for ln in lines if ln.startswith("Compiling "))
    n_arch = sum(1 for ln in lines if ln.startswith("Archiving "))
    return [
        "$ pio run -e vqeaf_os",
        *head,
        *comp,
        f"        ... {n_comp} d\u00f2ng Compiling / {n_arch} d\u00f2ng Archiving ...",
        *tail,
    ]


def build_upload_pio_excerpt():
    lines = (LOGS / "upload_pio.log").read_text(encoding="utf-8").splitlines()
    out = ["$ pio run -e vqeaf_os -t upload"]
    out += pick(lines, [
        r"^Configuring upload protocol", r"^CURRENT:", r"^Auto-detected:",
        r"^Uploading ", r"^esptool\.py v", r"^Serial port ", r"^Chip is ",
        r"^Features:", r"^Crystal is", r"^MAC: ", r"^Uploading stub", r"^Running stub",
        r"^Stub running", r"^Changing baud rate", r"^Configuring flash size",
    ])
    writes = [ln for ln in lines if ln.startswith("Wrote ")]
    hashes = [ln for ln in lines if ln.startswith("Hash of data verified")]
    compressed = [ln for ln in lines if ln.startswith("Compressed ")]
    for i, w in enumerate(writes):
        out.append(compressed[i] if i < len(compressed) else "")
        out.append(w)
        if i < len(hashes):
            out.append(hashes[i])
    out += pick(lines, [r"^Leaving\.\.\.", r"^Hard resetting", r"\[SUCCESS\]"])
    return [ln for ln in out if ln]


def build_upload_img_excerpt():
    lines = (LOGS / "upload_img.log").read_text(encoding="utf-8").splitlines()
    out = ["$ esptool.py --chip esp32s3 --port COM3 --baud 460800 write_flash 0x0 \\",
           "      dist/vqeaf_os_v2.3.4_merged.img"]
    out += pick(lines, [
        r"^esptool\.py v", r"^Serial port ", r"^Connecting", r"^Chip is ",
        r"^Features:", r"^Crystal is", r"^MAC: ", r"^Uploading stub", r"^Running stub",
        r"^Stub running", r"^Changing baud rate", r"^Changed\.", r"^Configuring flash size",
        r"^Flash will be erased",
    ])
    comp_line = next((ln for ln in lines if ln.startswith("Compressed ")), "")
    if comp_line:
        out.append(comp_line)
    first_write = next((ln for ln in lines if ln.startswith("Writing at 0x00000000")), "")
    if first_write:
        out.append(first_write)
    out.append("        ... Writing at ... (100 %) ...")
    out += pick(lines, [
        r"^Wrote ", r"^Hash of data verified",
        r"^Leaving\.\.\.", r"^Hard resetting",
    ])
    return [ln for ln in out if ln]


def build_boot_excerpt():
    lines = (LOGS / "boot_after_img.log").read_text(encoding="utf-8").splitlines()
    return [ln for ln in lines if ln.strip()]


# --------------------------------------------------------------------------
# flash layout diagram
# --------------------------------------------------------------------------

FULL = [
    ("bootloader", 0x000000, 0x003B00, C_BOOT),
    ("partitions", 0x008000, 0x008C00, C_PART),
    ("NVS", 0x009000, 0x00E000, C_NVS),
    ("boot_app0", 0x00E000, 0x010000, C_OTA0),
    ("app0  \u2014  firmware", 0x010000, 0x650000, C_OTA0),
    ("app1  \u2014  slot OTA d\u1ef1 ph\u00f2ng", 0x650000, 0xC90000, C_EMPTY),
    ("spiffs  \u2014  LittleFS", 0xC90000, 0xFF0000, C_SPIFFS),
    ("coredump", 0xFF0000, 0x1000000, C_CORE),
]

ZOOM = [
    ("bootloader", 0x0000, 0x3B00, C_BOOT),
    ("tr\u1ed1ng (0xFF)", 0x3B00, 0x8000, C_EMPTY),
    ("partitions", 0x8000, 0x8C00, C_PART),
    ("tr\u1ed1ng (0xFF)", 0x8C00, 0x9000, C_EMPTY),
    ("NVS", 0x9000, 0xE000, C_NVS),
    ("boot_app0", 0xE000, 0x10000, C_OTA0),
]

FLASH_TOTAL = 0x1000000


def draw_layout(path: Path) -> None:
    W, H = 1600, 760
    img = Image.new("RGB", (W, H), PANEL)
    d = ImageDraw.Draw(img)

    f_title = font(SEGOE, 30)
    f_sub = font(SEGOE, 17)
    f_lbl = font(SEGOE, 15)
    f_small = font(SEGOE, 13)
    f_mono = font(CONSOLA, 14)
    f_mono_b = font(CONSOLA_B, 15)
    f_marker = font(SEGOE, 14)

    x0, x1 = 60, W - 60
    span = x1 - x0

    def px(addr, total):
        return x0 + span * addr / total

    def draw_bar(y, h, regions, total):
        for _n, a, b, colour in regions:
            ax, bx = px(a, total), px(b, total)
            d.rectangle([ax, y, max(bx, ax + 1.5), y + h], fill=colour, outline="#FFFFFF")
        d.rectangle([x0, y, x1, y + h], outline=FRAME)

    def marker(x, y, n):
        r = 13
        d.ellipse([x - r, y - r, x + r, y + r], fill="#12263A")
        d.text((x, y), str(n), font=f_marker, fill="#FFFFFF", anchor="mm")

    d.text((40, 28), "VQEAF OS \u2014 s\u01a1 \u0111\u1ed3 b\u1ed9 nh\u1edb flash 16 MiB",
           font=f_title, fill=INK)
    d.text((40, 68), "B\u1ed1 c\u1ee5c th\u1eadt c\u1ee7a chip v\u00e0 v\u00f9ng b\u1ecb ghi "
                     "b\u1edfi t\u1eebng c\u00e1ch n\u1ea1p.", font=f_sub, fill=MUTED)

    # ---- panel A: whole chip, to scale -------------------------------------
    yA, hA = 146, 88
    d.text((x0, yA - 30), "To\u00e0n b\u1ed9 chip \u2014 16 MiB (\u0111\u00fang t\u1ec9 l\u1ec7)",
           font=f_mono_b, fill=INK)
    draw_bar(yA, hA, FULL, FLASH_TOTAL)
    for name, a, b, colour in FULL:
        size = b - a
        if size < 0x40000:            # too narrow to carry text
            continue
        cx = (px(a, FLASH_TOTAL) + px(b, FLASH_TOTAL)) / 2
        fg = "#1B1B1B" if colour in (C_EMPTY, C_SPIFFS, C_CORE) else "#FFFFFF"
        d.text((cx, yA + 30), name, font=f_lbl, fill=fg, anchor="mm")
        d.text((cx, yA + 52), f"0x{a:06X} + {size / 1048576:.2f} MiB",
               font=f_mono, fill=fg, anchor="mm")
    d.text((x0, yA + hA + 8), "0x000000", font=f_mono, fill=MUTED)
    d.text((x1, yA + hA + 8), "0x1000000", font=f_mono, fill=MUTED, anchor="ra")

    zx = px(0x10000, FLASH_TOTAL)
    d.line([zx, yA + hA + 4, zx, yA + hA + 40], fill=C_BOOT, width=2)
    d.text((zx + 10, yA + hA + 34), "\u2193 ph\u00f3ng to b\u00ean d\u01b0\u1edbi",
           font=f_small, fill=C_BOOT)

    # ---- panel B: zoom of the first 64 KiB --------------------------------
    yB, hB = 330, 76
    d.text((x0, yB - 30), "Ph\u00f3ng to 0x00000 \u2013 0x10000 (64 KiB)",
           font=f_mono_b, fill=INK)
    draw_bar(yB, hB, ZOOM, 0x10000)
    for i, (_n, a, b, _c) in enumerate(ZOOM, start=1):
        marker((px(a, 0x10000) + px(b, 0x10000)) / 2, yB + hB / 2, i)
    for addr in (0x0000, 0x4000, 0x8000, 0xC000, 0x10000):
        tx = px(addr, 0x10000)
        d.line([tx, yB + hB, tx, yB + hB + 8], fill=MUTED, width=1)
        d.text((tx, yB + hB + 12), f"0x{addr:04X}", font=f_mono, fill=MUTED, anchor="ma")

    # ---- legend ------------------------------------------------------------
    d.text((x0, 452), "Ch\u00fa gi\u1ea3i v\u00f9ng ph\u00f3ng to", font=f_lbl, fill=INK)
    # light region fills are unreadable as text, so fall back to a muted ink
    for idx, (name, a, b, colour) in enumerate(ZOOM):
        col, row = divmod(idx, 3)
        lx = 70 + col * 740
        ly = 484 + row * 34
        marker(lx + 13, ly, idx + 1)
        ink = MUTED if colour in (C_EMPTY, C_SPIFFS, C_CORE) else colour
        d.text((lx + 36, ly), name, font=f_lbl, fill=ink, anchor="lm")
        d.text((lx + 250, ly), f"0x{a:04X} \u2013 0x{b:04X}", font=f_mono,
               fill=INK, anchor="lm")
        d.text((lx + 430, ly), f"{(b - a):,}".replace(",", ".") + " B",
               font=f_mono, fill=MUTED, anchor="lm")

    # ---- notes -------------------------------------------------------------
    yN = 600
    d.rectangle([40, yN - 16, W - 40, H - 30], outline=FRAME)
    notes = [
        ("pio run -e vqeaf_os -t upload  \u2014  ghi 4 v\u00f9ng ri\u00eang l\u1ebb:", INK),
        ("0x0000 bootloader      0x8000 partitions      0xE000 boot_app0      "
         "0x10000 app0", MUTED),
        ("esptool write_flash 0x0 merged.img  \u2014  ghi 1 l\u1ea7n t\u1eeb 0x0, "
         "ph\u1ee7 lu\u00f4n c\u1ea3 v\u00f9ng tr\u1ed1ng b\u1eb1ng 0xFF", C_OTA0),
        ("\u21d2 NVS n\u1eb1m trong v\u00f9ng tr\u1ed1ng, n\u00ean n\u1ea1p .img s\u1ebd "
         "xo\u00e1 c\u00e0i \u0111\u1eb7t \u0111\u00e3 l\u01b0u (theme, WiFi, ghi ch\u00fa)",
         "#C0392B"),
    ]
    yy = yN
    for text, colour in notes:
        d.text((60, yy), text, font=f_mono if text.startswith("0x") else f_lbl,
               fill=colour)
        yy += 23

    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)
    print(f"{path}  ({W}x{H})")


# --------------------------------------------------------------------------

def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)

    jobs = [
        ("01_build.png", build_build_excerpt(),
         "Git Bash  -  pio run -e vqeaf_os"),
        ("02_upload_pio.png", build_upload_pio_excerpt(),
         "Git Bash  -  pio run -e vqeaf_os -t upload"),
        ("03_upload_img.png", build_upload_img_excerpt(),
         "Git Bash  -  esptool write_flash 0x0 merged.img"),
        ("04_boot_uart.png", build_boot_excerpt(),
         "COM3 @ 115200  \u2014  log kh\u1edfi \u0111\u1ed9ng sau khi n\u1ea1p"),
    ]

    for name, lines, title in jobs:
        img = rtp.render(lines, title, scale=2, max_cols=118)
        out = OUT / name
        img.save(out)
        print(f"{out}  ({img.width}x{img.height}, {len(lines)} lines)")

    draw_layout(OUT / "05_flash_layout.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
