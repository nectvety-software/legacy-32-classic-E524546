#!/usr/bin/env python3
"""Regenerate v2.3.4 pixel assets from transparent PNGs (never legacy glyphs)."""
from pathlib import Path
import subprocess,sys
R=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(R/'tools/pixel_icon_assets/compile_pixel_icons.py')],check=True)
print('PASS: compiled pixel art RGB565-RLE into src/core/VqeafIconData.h')
