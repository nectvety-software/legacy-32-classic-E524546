#!/usr/bin/env python3
"""Layout-only preview derived from v1.8 UI geometry and theme colors.
Not a screenshot from a physical ST7789 or emulated firmware frame buffer.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
R=Path(__file__).resolve().parents[1]
P=R/'preview';P.mkdir(exist_ok=True)
W,H=240,320
fdir=Path('/usr/share/fonts/truetype/dejavu')
font=ImageFont.truetype(str(fdir/'DejaVuSansCondensed.ttf'),11)
bold=ImageFont.truetype(str(fdir/'DejaVuSansCondensed-Bold.ttf'),14)
big=ImageFont.truetype(str(fdir/'DejaVuSansCondensed-Bold.ttf'),24)
small=ImageFont.truetype(str(fdir/'DejaVuSansCondensed.ttf'),9)
def rgb565(v):
    return ((v>>11&31)*255//31, (v>>5&63)*255//63, (v&31)*255//31)
bg=rgb565(0x8e44);panel=rgb565(0xaee9);selected=rgb565(0xdf93);accent=rgb565(0xffe0);chrome=rgb565(0x2b63);dim=rgb565(0x2222)
def chrome_ui(title):
    im=Image.new('RGB',(W,H),bg);d=ImageDraw.Draw(im)
    d.rectangle((0,0,239,26),fill=chrome);d.text((5,4),title,font=bold,fill='white')
    text='12:51';bbox=d.textbbox((0,0),text,font=small);tw=bbox[2]-bbox[0]
    d.text(((240-tw)//2,9),text,font=small,fill='white')
    for i,h in enumerate([2,4,6,8]):d.rectangle((206+3*i,18-h,207+3*i,17),fill='white')
    d.rectangle((223,9,231,15),outline='white');d.rectangle((232,11,233,13),fill='white')
    d.rectangle((225,11,229,13),fill='white')
    return im,d
def softkeys(d,left,middle,right):
    d.rectangle((0,298,239,319),fill=panel)
    d.line((0,298,239,298),fill='white');d.line((80,301,80,315),fill=dim);d.line((160,301,160,315),fill=dim)
    d.text((4,303),left,font=font,fill=dim)
    box=d.textbbox((0,0),middle,font=font);d.text(((240-(box[2]-box[0]))//2,303),middle,font=font,fill=dim)
    box=d.textbbox((0,0),right,font=font);d.text((236-(box[2]-box[0]),303),right,font=font,fill=dim)

def calculator():
    im,d=chrome_ui('Calculator')
    d.rectangle((8,42,231,89),fill=panel,outline=dim)
    d.text((23,54),'0',font=big,fill='black')
    keys=[['7','8','9','/'],['4','5','6','*'],['1','2','3','-'],['C','0','=','+']]
    for r,row in enumerate(keys):
        for col,label in enumerate(row):
            x=10+col*56;y=99+r*43
            fill=selected if (r==0 and col==0) else panel
            d.rectangle((x,y,x+51,y+38),fill=fill,outline='white' if (r==0 and col==0) else dim)
            b=d.textbbox((0,0),label,font=big)
            d.text((x+(52-(b[2]-b[0]))//2,y+5),label,font=big,fill='black')
    d.text((10,278),'Options: . / backspace / clear',font=small,fill=dim)
    softkeys(d,'Options','Enter','Back')
    return im

def stopwatch():
    im,d=chrome_ui('Stopwatch')
    d.rectangle((12,55,227,128),fill=panel)
    d.text((28,75),'00:00.00',font=big,fill='black')
    for i,label in enumerate(['Start','Lap','Reset']):
        x=8+i*76;fill=selected if i==0 else panel
        d.rectangle((x,140,x+69,172),fill=fill,outline='white' if i==0 else dim)
        b=d.textbbox((0,0),label,font=bold)
        d.text((x+(70-(b[2]-b[0]))//2,148),label,font=bold,fill='black')
    d.text((15,196),'4 laps max. Long-running timer.',font=small,fill=dim)
    d.text((15,216),'Use D-pad and Enter to begin.',font=small,fill=dim)
    softkeys(d,'','Select','Back')
    return im

def music():
    im,d=chrome_ui('Now playing')
    d.rectangle((15,47,224,170),fill=panel,outline=dim)
    d.rectangle((102,57,106,104),fill=(245,50,115))
    d.rectangle((105,57,138,61),fill=(245,50,115))
    d.ellipse((82,94,103,115),fill=(245,50,115))
    d.ellipse((115,87,136,108),fill=(245,50,115))
    d.text((24,181),'No track selected',font=bold,fill='black')
    d.text((25,210),'Left/Right track | Up/Down volume',font=small,fill=dim)
    d.text((27,223),'00:00 / 00:00',font=small,fill=dim)
    d.rectangle((24,243,213,252),outline=dim)
    d.text((26,260),'Volume 80%',font=small,fill=dim)
    d.rectangle((24,277,213,286),outline=dim)
    d.rectangle((26,279,175,284),fill=accent)
    softkeys(d,'Options','Play','List')
    return im

screens={'calculator':calculator(),'stopwatch':stopwatch(),'music_now_playing':music()}
for name,im in screens.items():
    im.save(P/f'v18_{name}_240x320.png')
contact=Image.new('RGB',(W*3,320),'#333333')
for i,im in enumerate(screens.values()):contact.paste(im,(i*W,0))
contact.resize((W*6,640),Image.Resampling.NEAREST).save(P/'v18_app_preview_2x.png')
print('Preview:',P/'v18_app_preview_2x.png')
