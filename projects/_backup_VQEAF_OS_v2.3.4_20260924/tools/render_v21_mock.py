#!/usr/bin/env python3
"""Annotate an actual C++ LauncherView host primitive raster with font labels.
Not an LCD screenshot; the host fake TFT intentionally has no glyph renderer.
"""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'

def annotate(source,out,selected=0):
    im=Image.open(source).convert('RGB');d=ImageDraw.Draw(im)
    font=lambda s,b=False:ImageFont.truetype(BOLD if b else FONT,s)
    white='#F0F7FF';muted='#A4BAD1';accent='#55D2C6'
    def text(x,y,s,color=white,sz=10,bold=False):d.text((x,y),s,fill=color,font=font(sz,bold))
    text(6,4,'VQEAF OS',sz=10,bold=True)
    text(103,6,'12:34',sz=10)
    text(201,6,'SD',sz=10)
    text(16,43,'HM',sz=12,bold=True)
    text(60,33,'HOME',sz=15,bold=True)
    text(62,55,'<  TABS  >',color=muted,sz=10)
    text(199,36,'1/6',color=muted,sz=10)
    rows=['Qeafbrowser','Installed apps','File manager','Media library','Themes','Recent tasks']
    marks=['WB','AP','FI','MD','TH','RC']
    off=0
    for r in range(5):
        col=white if r==selected else '#D5E6F7'
        text(13,91+r*29,marks[r],sz=9,bold=True)
        text(42,93+r*29,rows[r],color=col,sz=12,bold=r==selected)
    text(19,261,marks[selected],sz=13,bold=True)
    text(57,245,rows[selected],sz=12,bold=True)
    summary=['Lightweight browser','Signed QEAPP/2','Storage','Gallery and music','Select .vqeaf','System history']
    text(57,266,summary[selected],sz=10)
    text(6,303,'OPTION',sz=10,bold=True);text(91,303,'OK Open',sz=10,bold=True);text(187,303,'A Back',sz=10,bold=True)
    im.save(out)
    im.resize((720,960),Image.Resampling.NEAREST).save(str(Path(out).with_name(Path(out).stem+'_3x.png')))
    return out

if __name__=='__main__':
    annotate(ROOT/'preview/v21_host_home.ppm',ROOT/'preview/v21_launcher_home_mock.png')
    annotate(ROOT/'preview/v21_host_selected.ppm',ROOT/'preview/v21_launcher_selected_mock.png',selected=1)
    print('Created source-layout mock previews (not hardware screenshots).')
