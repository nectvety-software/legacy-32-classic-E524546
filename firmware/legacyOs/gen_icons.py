#!/usr/bin/env python3
"""Convert Material Design SVG icons to RGB565 C arrays for TFT_eSPI.
   Uses svg.path for proper path parsing with winding-rule hole handling."""

import urllib.request
import re, math, sys
from PIL import Image, ImageDraw
from svg.path import parse_path, Move, Close, Line, CubicBezier, QuadraticBezier, Arc

SVG_NAMES = ['folder', 'settings', 'info', 'wifi', 'wb_sunny', 'developer_board', 'build', 'memory']
APP_KEYS  = ['folder', 'settings', 'info', 'wifi', 'wb_sunny', 'developer_board', 'build', 'memory']

def signed_area(poly):
    area = 0.0
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        area += x1 * y2 - x2 * y1
    return area / 2.0

def point_in_polygon(x, y, poly):
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if ((yi > y) != (yj > y)) and (x < (xj - xi) * (y - yi) / (yj - yi) + xi):
            inside = not inside
        j = i
    return inside

def path_to_contours(path_str):
    """Parse SVG path string, return list of contours (list of (x,y) tuples)."""
    path = parse_path(path_str)
    contours = []
    current = []
    for seg in path:
        if isinstance(seg, Move):
            if current and len(current) > 1:
                contours.append(current)
            current = [(seg.end.real, seg.end.imag)]
        else:
            if isinstance(seg, (Line, Close)):
                steps = 2
            elif isinstance(seg, (QuadraticBezier, Arc)):
                steps = 6
            else:
                steps = 8
            for t in range(1, steps + 1):
                pt = seg.point(t / steps)
                current.append((pt.real, pt.imag))
    if current and len(current) > 1:
        contours.append(current)
    return contours

def rasterize_svg(path_str, size=24):
    """Render SVG path to 24x24 b/w image with proper hole handling."""
    contours = path_to_contours(path_str)
    img = Image.new('L', (size, size), 0)
    draw = ImageDraw.Draw(img)

    # Classify contours
    outer_contours = []
    hole_contours = []
    for c in contours:
        if len(c) < 3:
            continue
        area = signed_area(c)
        if area < 0:
            outer_contours.append(c)
        else:
            hole_contours.append(c)

    # Draw outer contours (fill white)
    for c in outer_contours:
        draw.polygon(c, fill=255)

    # For holes: check if inside any outer contour
    for c in hole_contours:
        cx = sum(p[0] for p in c) / len(c)
        cy = sum(p[1] for p in c) / len(c)
        is_hole = any(point_in_polygon(cx, cy, outer) for outer in outer_contours)
        if is_hole:
            draw.polygon(c, fill=0)
        else:
            draw.polygon(c, fill=255)

    return img

def fetch_svg(icon_name):
    try:
        url = f"https://material-icons.github.io/material-icons/svg/{icon_name}/baseline.svg"
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        svg = urllib.request.urlopen(req, timeout=10).read().decode('utf-8')
        return svg
    except Exception as e:
        return None

def extract_path_from_svg(svg_text):
    m = re.search(r'<path[^>]*\sd="([^"]*)"', svg_text)
    if m: return m.group(1)
    m = re.search(r"<path[^>]*\sd='([^']*)'", svg_text)
    if m: return m.group(1)
    return None

def img_to_c_array(img, name):
    w, h = img.size
    pixels = []
    for y in range(h):
        for x in range(w):
            p = img.getpixel((x, y))
            if p > 128:
                r, g, b = 255, 255, 255
            else:
                r, g, b = 0, 0, 0
            rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            pixels.append(f"0x{rgb565:04X}")
    lines = [f"// Material Icon: {name}",
             f"static const uint16_t icon_{name}[{w}*{h}] PROGMEM = {{"]
    for i in range(0, len(pixels), 12):
        lines.append("    " + ", ".join(pixels[i:i+12]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"

def main():
    print("// Auto-generated Material Design Icon Arrays for LegacyOs")
    print("// Source: https://material-icons.github.io/material-icons/")
    print("#include <Arduino.h>")
    print("#include <pgmspace.h>")
    print()

    for svg_name, key in zip(SVG_NAMES, APP_KEYS):
        print(f"// --- {key} ({svg_name}) ---", file=sys.stderr)
        svg = fetch_svg(svg_name)
        if svg:
            path_d = extract_path_from_svg(svg)
            if path_d:
                print(f"  Got SVG, path={len(path_d)} chars", file=sys.stderr)
                img = rasterize_svg(path_d, 24)
                print(img_to_c_array(img, key))
                continue
        print(f"  FAILED for {svg_name}, using fallback", file=sys.stderr)
        img = Image.new('L', (24, 24), 0)
        draw = ImageDraw.Draw(img)
        cx, cy = 12, 12
        if key == 'folder':
            draw.rectangle([2, 8, 21, 20], fill=255)
            draw.rectangle([2, 4, 11, 8], fill=255)
        elif key == 'settings':
            draw.ellipse([5, 5, 19, 19], fill=255)
            draw.ellipse([8, 8, 16, 16], fill=0)
            for a in range(0, 360, 45):
                r = math.radians(a)
                x1 = cx + int(7 * math.cos(r))
                y1 = cy + int(7 * math.sin(r))
                x2 = cx + int(10 * math.cos(r))
                y2 = cy + int(10 * math.sin(r))
                draw.line([x1, y1, x2, y2], fill=255, width=2)
        elif key == 'info':
            draw.ellipse([2, 2, 21, 21], fill=255)
            draw.rectangle([10, 8, 14, 10], fill=0)
            draw.rectangle([10, 12, 14, 18], fill=0)
        elif key == 'wifi':
            for r2 in [10, 7, 4]:
                draw.arc([cx-r2, cy-r2, cx+r2, cy+r2], -60, 60, fill=255, width=2)
            draw.ellipse([cx-2, cy+6, cx+2, cy+10], fill=255)
        elif key == 'wb_sunny':
            draw.ellipse([4, 4, 20, 20], fill=255)
            for a in range(0, 360, 30):
                r = math.radians(a)
                x1 = cx + int(10 * math.cos(r))
                y1 = cy + int(10 * math.sin(r))
                x2 = cx + int(12 * math.cos(r))
                y2 = cy + int(12 * math.sin(r))
                draw.line([x1, y1, x2, y2], fill=255, width=2)
        elif key == 'developer_board':
            draw.rectangle([4, 4, 20, 20], fill=255)
            draw.rectangle([6, 7, 10, 11], fill=0)
            draw.rectangle([14, 7, 18, 11], fill=0)
            draw.rectangle([6, 13, 10, 17], fill=0)
            draw.rectangle([14, 13, 18, 17], fill=0)
        elif key == 'build':
            draw.rectangle([6, 2, 9, 14], fill=255)
            draw.rectangle([3, 12, 12, 15], fill=255)
            draw.rectangle([12, 8, 15, 20], fill=255)
            draw.rectangle([10, 18, 20, 21], fill=255)
        elif key == 'memory':
            draw.rectangle([3, 3, 21, 21], fill=255)
            draw.rectangle([5, 5, 19, 19], fill=0)
            for x in [7, 11, 15]:
                draw.rectangle([x, 5, x+2, 7], fill=255)
                draw.rectangle([x, 17, x+2, 19], fill=255)
            for y in [7, 11, 15]:
                draw.rectangle([5, y, 7, y+2], fill=255)
                draw.rectangle([17, y, 19, y+2], fill=255)
        print(img_to_c_array(img, key))

    print("// Icon lookup table")
    print("static const uint16_t* icon_data[] = {")
    for key in APP_KEYS:
        print(f"    icon_{key},")
    print("};")
    print("#define ICON_COUNT", len(APP_KEYS))

if __name__ == '__main__':
    main()
