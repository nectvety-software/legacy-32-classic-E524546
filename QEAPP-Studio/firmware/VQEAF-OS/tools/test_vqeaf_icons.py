#!/usr/bin/env python3
"""Production icon artifacts are consistent with the supplied pixel PNG assets."""
from pathlib import Path
import subprocess,sys,hashlib
R=Path(__file__).resolve().parents[1]
asset=R/'src/core/VqeafIconData.h'
before=hashlib.sha256(asset.read_bytes()).hexdigest()
subprocess.run([sys.executable,str(R/'tools/pixel_icon_assets/compile_pixel_icons.py')],check=True)
assert before==hashlib.sha256(asset.read_bytes()).hexdigest(),'RLE source was stale'
subprocess.run([sys.executable,str(R/'tools/test_pixel_icons_v234.py')],check=True)
print('PASS: 24 RGB565 pixel assets, source hash stable')
