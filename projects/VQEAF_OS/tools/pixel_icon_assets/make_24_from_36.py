#!/usr/bin/env python3
"""Produce optically consistent 24px list assets from 36px menu art."""
from pathlib import Path
from PIL import Image
p=Path(__file__).resolve().parent/'png'
for img in sorted(p.glob('*_36.png')):
 name=img.stem[:-3];source=Image.open(img).convert('RGBA')
 a=source.getchannel('A');box=a.getbbox();assert box,(img,'no subject')
 subject=source.crop(box)
 scale=min(18/subject.width,18/subject.height)
 w,h=round(subject.width*scale),round(subject.height*scale)
 # nearest neighbor with alpha kept exactly 0 or 255 to avoid background halos
 output=Image.new('RGBA',(24,24),(0,0,0,0))
 output.alpha_composite(subject.resize((w,h),Image.Resampling.NEAREST),((24-w)//2,(24-h)//2))
 output.save(p/f'{name}_24.png',optimize=True)
 print(f'{name}: 36 -> 24 (content {w}x{h})')
