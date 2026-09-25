#!/usr/bin/env python3
"""Rebuild source GUI on host; exact-size 240x320 Home/Menu previews & icon parity.

Not a firmware build and not a hardware test. Requires C++ g++ and Pillow.
"""
from pathlib import Path
import subprocess, sys, tempfile, hashlib
from PIL import Image, ImageDraw, ImageFont
R = Path(__file__).resolve().parents[1]

def run(*args):
    p=subprocess.run([str(a) for a in args],cwd=R,text=True,
                     stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if p.stdout.strip():print(p.stdout.strip()[-3200:])
    if p.returncode:raise SystemExit('FAILED: '+' '.join(map(str,args)))

# Protect against generated renderer assets becoming stale.
run(sys.executable,'tools/test_ui_icon_contract_v232.py')
run(sys.executable,'tools/test_vqeaf_icons.py')
with tempfile.TemporaryDirectory(prefix='vqeaf-v232-') as t:
    dst=Path(t);bin=dst/'icon_routes';
    run('g++','-std=c++11','-Wall','-Wextra','-Werror','-fpermissive',
        '-Itools/vqeaf_host/reference_stubs','-Itools/host_stubs','-Iinclude',
        '-Isrc','-Isrc/core','tools/vqeaf_host/test_icon_routes.cpp',
        'src/core/SymbianUI.cpp','src/core/VqeafIconRenderer.cpp',
        'tools/host_stubs/host_globals.cpp','-o',bin)
    run(bin,str(dst)+'/')
    preview=R/'preview';preview.mkdir(exist_ok=True)
    for name in ['home','menu','home_selected','menu_selected']:
        im=Image.open(dst/f'{name}.ppm').convert('RGB')
        assert im.size==(240,320),(name,im.size)
        path=preview/f'v234_{name}_240x320.png'
        im.save(path,optimize=True)
        print(f'PASS generated {path.relative_to(R)} 240x320 sha256={hashlib.sha256(path.read_bytes()).hexdigest()[:16]}')

# Handy comparison contact sheet kept distinct from native-size renders.
files=['home','menu','home_selected','menu_selected']
sheet=Image.new('RGB',(2*240+24,2*(320+30)+10),'#202923')
d=ImageDraw.Draw(sheet)
font=ImageFont.load_default()
for index,name in enumerate(files):
    x=8+(index%2)*248;y=4+(index//2)*350
    d.text((x,y),f'HOST C++: {name.upper()}',fill='white',font=font)
    sheet.paste(Image.open(R/f'preview/v234_{name}_240x320.png').convert('RGB'),(x,y+23))
sheet.save(R/'preview/v234_home_menu_contact_sheet.png')
# Run older full GUI/regression suite. This also checks icon/palette alignment.
run(sys.executable,'tools/test_ui_v23.py')
print('PASS v2.3.4 new pixel assets on production Home/Menu. PlatformIO and device unverified.')
