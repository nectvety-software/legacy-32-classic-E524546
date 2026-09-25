#!/usr/bin/env python3
"""Reference 240x320 UI geometry mock; not a hardware screenshot."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parent.parent
PREVIEW=ROOT/"preview"
PREVIEW.mkdir(exist_ok=True)
fontdir=Path('/usr/share/fonts/truetype/dejavu')
font=ImageFont.truetype(str(fontdir/'DejaVuSansCondensed-Bold.ttf'),12)
small=ImageFont.truetype(str(fontdir/'DejaVuSansCondensed.ttf'),11)
medium=ImageFont.truetype(str(fontdir/'DejaVuSansCondensed-Bold.ttf'),11)
bg=(141,202,35)
header=(42,107,27)
bar=(183,233,132)
canvas=Image.new('RGB',(240,320),bg);d=ImageDraw.Draw(canvas)
d.rectangle((0,0,239,26),fill=header)
d.line((0,25,239,25),fill=(119,177,78))
d.text((4,6),'App inbox',fill='white',font=font)
time='10:00';tw=d.textbbox((0,0),time,font=small)[2]
d.text(((240-tw)//2,7),time,fill='white',font=small)
for i,h in enumerate((2,4,6,8)):
 d.rectangle((206+i*3,16-h,207+i*3,15),fill='white')
d.rectangle((223,8,231,14),outline='white')
d.rectangle((232,10,233,12),fill='white')
d.rectangle((225,10,229,12),fill='white')
# Verified package detail screen follows SymbianUI::message() positions.
d.text((12,50),'Verified package',fill='black',font=font)
d.text((12,86),'Welcome',fill='black',font=small)
d.text((12,106),'Version 1.0.0  /  text',fill='black',font=small)
d.text((12,126),'Permission: read bundled text',fill='black',font=small)
icon=Image.open(ROOT/'sd/System/Apps/Inbox/welcome_icon.png').convert('RGB')
canvas.paste(icon,(103,170))
# S60 softkey regions.
d.rectangle((0,298,239,319),fill=bar)
d.line((0,298,239,298),fill='white')
d.line((80,302,80,317),fill=(130,180,80));d.line((160,302,160,317),fill=(130,180,80))
d.text((4,302),'Options',fill=(25,77,21),font=medium)
word='Install';w=d.textbbox((0,0),word,font=medium)[2]
d.text(((240-w)//2,302),word,fill=(25,77,21),font=medium)
word='Back';w=d.textbbox((0,0),word,font=medium)[2]
d.text((236-w,302),word,fill=(25,77,21),font=medium)
p=PREVIEW/'v14_qeapp_installer_240x320.png';canvas.save(p)
canvas.resize((480,640),Image.Resampling.NEAREST).save(PREVIEW/'v14_qeapp_installer_2x.png')
print(p)
