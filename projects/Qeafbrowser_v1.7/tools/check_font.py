#!/usr/bin/env python3
"""Extract glyph bytes from lc_font_*.h and walk like C lc_find_glyph."""
from __future__ import annotations
import re
import sys
from pathlib import Path


def c_div(a: int, b: int) -> int:
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def extract_glyph_bytes(text: str) -> list[int]:
    # Grab only the innermost { ... } after "413, {" (the data array).
    m = re.search(r"static const LcFont\s+\w+\s*=\s*\{", text)
    if not m:
        raise SystemExit("no LcFont")
    rest = text[m.end():]
    # data array starts after chars count: pattern  <n>, {
    m2 = re.search(r",\s*\d+\s*,\s*\{", rest)
    if not m2:
        raise SystemExit("no data array")
    start = m2.end()
    # find matching close for this brace
    depth = 1
    i = start
    while i < len(rest) and depth:
        if rest[i] == "{":
            depth += 1
        elif rest[i] == "}":
            depth -= 1
        i += 1
    body = rest[start : i - 1]
    vals = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", body)]
    return vals


def walk(data: list[int], label: str, fix_empty: bool) -> None:
    i = 0
    n = len(data)
    glyphs: list[tuple] = []
    while i + 1 < n:
        code = data[i] | (data[i + 1] << 8)
        if code == 0:
            print(f"{label}: terminator @{i}, tail={n - i}")
            break
        if i + 6 >= n:
            print(f"{label}: truncated header @{i}")
            break
        y, w, h, ox, adv = data[i + 2], data[i + 3], data[i + 4], data[i + 5], data[i + 6]
        if w == 0 or h == 0:
            blen = 0 if fix_empty else (c_div((w * h) - 1, 8) + 1)
        else:
            blen = c_div((w * h) - 1, 8) + 1
        glyphs.append((code, y, w, h, ox, adv, blen, i))
        i += 7 + blen
    else:
        print(f"{label}: ran off end i={i} n={n}")
    print(f"{label}: n_glyphs={len(glyphs)} end={i}/{n} fix_empty={fix_empty}")
    if glyphs:
        print("  first5:", [(hex(g[0]), g[1:6]) for g in glyphs[:5]])
        print("  last3:", [(hex(g[0]), g[1:6]) for g in glyphs[-3:]])
    keys = [0x20, 0x43, 0x61, 0xE0, 0xEC, 0x1EC7, 0x1EDD, 0x1EE9, 0x01A1, 0x01B0, 0x1ED7]
    found = {g[0] for g in glyphs}
    print("  want:", {hex(k): (k in found) for k in keys})


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    for name in ("lc_font_vn12.h", "lc_font_vn16.h"):
        p = root / "include" / "fonts" / name
        data = extract_glyph_bytes(p.read_text(encoding="utf-8", errors="replace"))
        print(f"\n== {name} raw_bytes={len(data)} ==")
        walk(data, name + "/C", fix_empty=False)
        walk(data, name + "/fix", fix_empty=True)


if __name__ == "__main__":
    main()
