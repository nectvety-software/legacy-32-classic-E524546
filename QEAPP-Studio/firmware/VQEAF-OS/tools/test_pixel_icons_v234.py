#!/usr/bin/env python3
"""Exercise the real C++ renderer, then compare each RGB565 framebuffer pixel
against independently recompiled source PNGs (not a simulated renderer).
"""
from pathlib import Path
from PIL import Image
import importlib.util,subprocess,tempfile,json
R=Path(__file__).resolve().parents[1]
P=R/'tools/pixel_icon_assets/compile_pixel_icons.py'
sp=importlib.util.spec_from_file_location('pixels',P)
pixels=importlib.util.module_from_spec(sp);sp.loader.exec_module(pixels)

def run(*args):
 p=subprocess.run([str(v) for v in args],cwd=R,text=True,capture_output=True)
 if p.returncode:
  print(p.stdout,p.stderr);raise SystemExit('ERROR: '+' '.join(map(str,args)))
 print(p.stdout.strip())

with tempfile.TemporaryDirectory(prefix='vqeaf-234-icon-') as tmp:
 td=Path(tmp)
 run('g++','-std=c++11','-Wall','-Wextra','-Werror',
     '-Itools/icon_host','-Isrc/core','src/core/VqeafIconRenderer.cpp',
     'tools/icon_host/test_icons.cpp','-o',td/'icon_host')
 run(td/'icon_host',td)
 checked=0
 for n in (24,36):
  for name in pixels.NAMES:
   palette,_,indices=pixels.compile_image(name,n)
   im=Image.open(td/f'{name.capitalize() if name!="wifi" else "WiFi"}_{n}.ppm').convert('RGB')
   assert im.size==(n,n)
   for i,role in enumerate(indices):
    value=0xAEE9 if role==0 else palette[role]
    assert im.getpixel((i%n,i//n))==pixels.unpack565(value),(name,n,i)
    checked+=1
print('PASS: exact RGB565 pixel parity',checked,'framebuffer pixels on 24 source assets')
