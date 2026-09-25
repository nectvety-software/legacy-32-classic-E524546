#!/usr/bin/env python3
"""Render a captured terminal log as a PNG, for embedding in documentation.

This draws the *real* captured output of a command; it never invents text.
Feed it a log file saved from the actual run.

Usage:
    py -3 tools/render_terminal_png.py build.log -o out.png \
        --title "pio run -e vqeaf_os" --tail 24

    py -3 tools/render_terminal_png.py upload.log -o up.png \
        --title "pio run -e vqeaf_os -t upload" --drop-regex "^\\[safe-delete\\]"

Requires Pillow.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_REGULAR = Path("C:/Windows/Fonts/consola.ttf")
FONT_BOLD = Path("C:/Windows/Fonts/consolab.ttf")

BG = "#0C0C0C"
CHROME = "#2B2B2B"
CHROME_TEXT = "#C8C8C8"
FG = "#D4D4D4"
DIM = "#7A7A7A"
GREEN = "#4EC94E"
RED = "#F14C4C"
CYAN = "#4FC1FF"
YELLOW = "#E5C07B"
PROMPT = "#7FD1B9"

# (regex, colour, bold). First match wins.
RULES = (
    (re.compile(r"\bFAILED\b|error:|Error \d|\bFAIL\b|Traceback"), RED, True),
    (re.compile(r"\[SUCCESS\]|Hash of data verified|verified\."), GREEN, True),
    (re.compile(r"Chip is |MAC:|Auto-detected|CURRENT:|Hard resetting"), CYAN, True),
    (re.compile(r"^\s*\$ |^> "), PROMPT, True),
    (re.compile(r"^Writing at "), DIM, False),
    (re.compile(r"^(Compiling|Archiving) "), DIM, False),
    (re.compile(r"^Wrote .* in .* seconds"), FG, False),
    (re.compile(r"^ESP-ROM:|^rst:0x|^entry 0x|^boot:0x"), YELLOW, False),
    (re.compile(r"\[VQEAF\]|\[S3DIAG\]"), CYAN, True),
    (re.compile(r"\[E\]\["), RED, False),
)


def pick_colour(line: str):
    for pattern, colour, bold in RULES:
        if pattern.search(line):
            return colour, bold
    return FG, False


def load_lines(path: Path, tail: int | None, head: int | None, drop: re.Pattern | None):
    raw = path.read_text(encoding="utf-8", errors="replace").splitlines()
    if drop is not None:
        raw = [ln for ln in raw if not drop.search(ln)]
    # drop trailing blank lines so the image does not end with dead space
    while raw and not raw[-1].strip():
        raw.pop()
    if head is not None:
        raw = raw[:head]
    if tail is not None:
        raw = raw[-tail:]
    return raw


def render(lines, title: str, scale: int, max_cols: int) -> Image.Image:
    font = ImageFont.truetype(str(FONT_REGULAR), 13 * scale)
    bold_font = ImageFont.truetype(str(FONT_BOLD), 13 * scale)
    title_font = ImageFont.truetype(str(FONT_BOLD), 12 * scale)

    shown = []
    for ln in lines:
        shown.append(ln if len(ln) <= max_cols else ln[: max_cols - 1] + "\u2026")

    pad = 12 * scale
    line_h = 18 * scale
    chrome_h = 30 * scale if title else 0

    text_w = 0
    probe = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    for ln in shown:
        text_w = max(text_w, int(probe.textlength(ln, font=font)))
    text_w = max(text_w, int(probe.textlength(title, font=title_font)))

    width = text_w + pad * 2
    height = chrome_h + pad * 2 + line_h * len(shown)

    img = Image.new("RGB", (width, height), BG)
    draw = ImageDraw.Draw(img)

    if title:
        draw.rectangle([0, 0, width, chrome_h], fill=CHROME)
        for i, dot in enumerate(("#FF5F56", "#FFBD2E", "#27C93F")):
            cx = (14 + i * 18) * scale
            r = 6 * scale
            draw.ellipse([cx - r, chrome_h // 2 - r, cx + r, chrome_h // 2 + r], fill=dot)
        draw.text((78 * scale, chrome_h // 2), title, font=title_font,
                  fill=CHROME_TEXT, anchor="lm")

    y = chrome_h + pad
    for ln in shown:
        colour, bold = pick_colour(ln)
        draw.text((pad, y), ln, font=bold_font if bold else font, fill=colour)
        y += line_h

    return img


def main() -> int:
    ap = argparse.ArgumentParser(description="Render a terminal log as a PNG.")
    ap.add_argument("log", type=Path, help="captured log file")
    ap.add_argument("-o", "--out", type=Path, required=True, help="output PNG")
    ap.add_argument("--title", default="", help="window title bar text")
    ap.add_argument("--tail", type=int, default=None, help="keep only the last N lines")
    ap.add_argument("--head", type=int, default=None, help="keep only the first N lines")
    ap.add_argument("--drop-regex", default=None,
                    help="drop lines matching this regex (e.g. sandbox noise)")
    ap.add_argument("--scale", type=int, default=2, help="integer upscale, default 2")
    ap.add_argument("--max-cols", type=int, default=110, help="truncate lines longer than this")
    args = ap.parse_args()

    if not args.log.exists():
        raise SystemExit(f"no such log: {args.log}")

    drop = re.compile(args.drop_regex) if args.drop_regex else None
    lines = load_lines(args.log, args.tail, args.head, drop)
    if not lines:
        raise SystemExit("nothing to render after filtering")

    img = render(lines, args.title, args.scale, args.max_cols)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.out)
    print(f"{args.out}  ({img.width}x{img.height}, {len(lines)} lines)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
