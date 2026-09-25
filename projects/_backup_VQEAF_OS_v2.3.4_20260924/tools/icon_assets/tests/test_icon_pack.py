#!/usr/bin/env python3
"""Rebuild + compile real production icon renderer + compare bit-exact RGB565."""
import subprocess,tempfile,sys,importlib.util
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1]
S=importlib.util.spec_from_file_location('build_icons',R/'tools/build_icons.py')
mod=importlib.util.module_from_spec(S);S.loader.exec_module(mod)

def c(*args):
 p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,cwd=R)
 print(p.stdout.strip())
 if p.returncode:raise RuntimeError('Failed: '+' '.join(map(str,args)))

if __name__=='__main__':
 c(sys.executable,str(R/'tools/build_icons.py'))
 with tempfile.TemporaryDirectory(prefix='vqeaf-icon-') as td:
  out=Path(td);bin=out/'icons'
  c('g++','-std=c++11','-Wall','-Wextra','-Werror','-Itests','-Ifirmware',
    'firmware/VqeafIconRenderer.cpp','tests/test_icons.cpp','-o',str(bin))
  c(str(bin),str(out))
  inv={v:k for k,v in mod.TOK.items()}
  def rgb565(hex):
   r,g,b=(int(hex[i:i+2],16) for i in (1,3,5))
   return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)
  def rgb(c):return (((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31)
  for n in (24,36):
   for name,title,alias in mod.NAMES:
    image=Image.open(out/f'{title.replace("File manager","Files")}_{n}.ppm').convert('RGB')
    source=mod.design(name).raster(n)
    for off,pal_index in enumerate(source.getdata()):
     expect=0xAEE9 if pal_index==0 else rgb565(mod.HEX[inv[pal_index]])
     assert image.getpixel((off%n,off//n))==rgb(expect), (name,n,off)
  print('PASS pixel-identical 24 PNG/RLE pairs on RGB565 lime background.')
  print('PASS sizes, safety margins, transparent key, build C++11 -Werror.')
