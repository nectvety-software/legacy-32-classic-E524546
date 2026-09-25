#!/usr/bin/env python3
"""Short deterministic host verification; no ESP32 target or peripheral emulation."""
import subprocess,sys,tempfile
from pathlib import Path
r=Path(__file__).resolve().parent.parent

def check(cmd):
 p=subprocess.run([str(c) for c in cmd],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 if p.returncode: print(p.stdout[-5000:]);raise SystemExit(p.returncode)
 print(p.stdout.strip()[-1300:])

check([sys.executable,'tools/test_ui_icon_contract_v232.py'])
check([sys.executable,'tools/test_vqeaf_icons.py'])
with tempfile.TemporaryDirectory(prefix='vqeaf-v233-icon-') as tmp:
 out=Path(tmp)/'host_icon_routes'
 check(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fpermissive',
       '-Itools/vqeaf_host/reference_stubs','-Itools/host_stubs',
       '-Iinclude','-Isrc','-Isrc/core','tools/vqeaf_host/test_icon_routes.cpp',
       'src/core/SymbianUI.cpp','src/core/VqeafIconRenderer.cpp',
       'tools/host_stubs/host_globals.cpp','-o',out])
 check([out,str(Path(tmp))+'/'])
print('PASS fast host smoke: 24 icon variants and Home/Menu C++ GUI routing')
print('NOTE external Arduino/ESP32 headers and PlatformIO firmware not validated')
