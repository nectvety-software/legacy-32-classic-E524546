#!/usr/bin/env python3
"""240x320 UI illustrations (not ESP32 camera screenshots)."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
OUT=Path(__file__).resolve().parent.parent/'preview'
OUT.mkdir(exist_ok=True)
fontdir=Path('/usr/share/fonts/truetype/dejavu')
def font(name,size):
    try:return ImageFont.truetype(str(fontdir/name),size)
    except OSError:return ImageFont.load_default()
title=font('DejaVuSans-Bold.ttf',11)
small=font('DejaVuSans.ttf',9)
bold=font('DejaVuSans-Bold.ttf',10)

def render(good):
    W,H=240,320
    im=Image.new('RGB',(W,H),(140,204,37));d=ImageDraw.Draw(im)
    d.rectangle((0,0,239,26),fill=(41,104,25));d.text((4,5),'App inbox',fill='white',font=title)
    tm='10:00';tw=d.textbbox((0,0),tm,font=small)[2]
    d.text(((240-tw)//2,10),tm,fill='white',font=small)
    # same compact WiFi+battery glyph slots used by OS
    for n,h in enumerate((2,4,6,8)):
      d.rectangle((206+n*3,17-h,207+n*3,16),fill='white')
    d.rectangle((223,8,231,14),outline='white');d.rectangle((233,10,234,12),fill='white')
    d.line((0,26,239,26),fill=(196,238,112))
    if good:
      d.text((12,49),'Signature verified',fill='black',font=bold)
      d.text((12,86),'Welcome',fill='black',font=small)
      d.text((12,106),'Version 1.0.0 / text',fill='black',font=small)
      d.text((12,126),'Permission: bundled text',fill='black',font=small)
      d.rectangle((103,170,134,201),fill=(0,93,188),outline='white')
      d.rectangle((111,177,124,195),fill='white');d.line((113,183,121,183),fill=(0,123,188))
      center='Install';left='Options';right='Back'
    else:
      d.text((12,49),'Package rejected',fill=(110,15,26),font=bold)
      d.text((12,86),'Signature check failed',fill='black',font=small)
      d.text((12,106),'Digital signature',fill='black',font=small)
      d.text((12,123),'verification failed',fill='black',font=small)
      d.text((12,153),'No files were installed',fill='black',font=small)
      center='';left='';right='Back'
    d.rectangle((0,298,239,319),fill=(185,230,133));d.line((0,298,239,298),fill='white')
    d.line((80,301,80,317),fill=(128,170,92));d.line((160,301,160,317),fill=(128,170,92))
    d.text((4,303),left,fill=(28,76,24),font=bold)
    tw=d.textbbox((0,0),center,font=bold)[2];d.text(((240-tw)//2,303),center,fill=(28,76,24),font=bold)
    tw=d.textbbox((0,0),right,font=bold)[2];d.text((236-tw,303),right,fill=(28,76,24),font=bold)
    p=OUT/('v15_signature_verified_240x320.png' if good else 'v15_invalid_signature_240x320.png')
    im.save(p)
    return str(p)
if __name__=='__main__':
    print(render(True));print(render(False))
