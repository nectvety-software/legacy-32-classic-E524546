#!/usr/bin/env python3
"""Illustrative S60 Idle/Home 240x320 state previews, NOT LCD captures."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'preview';OUT.mkdir(exist_ok=True)
W,H=240,320
F=Path('/usr/share/fonts/truetype/dejavu')
regular=ImageFont.truetype(str(F/'DejaVuSansCondensed.ttf'),10)
bold=ImageFont.truetype(str(F/'DejaVuSansCondensed-Bold.ttf'),12)
large=ImageFont.truetype(str(F/'DejaVuSansCondensed-Bold.ttf'),28)
small=ImageFont.truetype(str(F/'DejaVuSansCondensed.ttf'),9)
white=(255,255,255)
def rgb565(c):return ((c>>11 & 31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31)
def tlen(d,s,f):b=d.textbbox((0,0),s,font=f);return b[2]-b[0]
def render(status,online,filename):
    band=[0x4BE5,0x6542,0x75A3,0x8E25,0x96A6,0x8E44,0xA6C8]
    im=Image.new('RGB',(W,H),rgb565(band[0]));d=ImageDraw.Draw(im)
    for i,c in enumerate(band):d.rectangle((0,i*46,239,min(319,(i+1)*46-1)),fill=rgb565(c))
    for i in range(5):
        d.line((0,260+i*3,150,170+i*5),fill=rgb565(0xB6EE))
        d.line((45,319-i*4,235,215+i*2),fill=rgb565(0x75A3))
    d.rectangle((0,0,239,26),fill=rgb565(0x2B63))
    d.line((0,25,239,25),fill=rgb565(0x4BE5));d.line((0,26,239,26),fill=rgb565(0xAEE9))
    d.text((4,5),'General',font=bold,fill=white)
    top='11:50';d.text(((W-tlen(d,top,small))//2,9),top,font=small,fill=white)
    if online:
        for i,h in enumerate((2,4,6,8)):d.rectangle((206+i*3,17-h,207+i*3,16),fill=white)
    d.rectangle((223,7,231,14),outline=white);d.rectangle((232,9,233,12),fill=white);d.rectangle((225,9,229,12),fill=white)
    # Clock standby card (S60 Green selection 0xDF93)
    d.rectangle((12,43,227,120),fill=rgb565(0xDF93),outline=white)
    d.text(((W-tlen(d,'11:50',large))//2,55),'11:50',font=large,fill='black')
    date='Thursday 24/09/2026';d.text(((W-tlen(d,date,regular))//2,94),date,font=regular,fill='black')
    panel=rgb565(0xB6EE)
    d.rectangle((12,132,227,177),fill=panel)
    limit=status
    while tlen(d,limit,small)>204:limit=limit[:-1]
    d.text((18,140),limit,font=small,fill='black')
    d.text((18,155),'Device ready',font=small,fill='black')
    for i,(label,glyph) in enumerate((('WiFi','W'),('Music','M'),('Files','F'))):
        x=8+i*76
        d.rectangle((x,191,x+71,257),fill=rgb565(0xDF93) if i==0 else rgb565(0xAEE9),outline=white if i==0 else rgb565(0x2222))
        d.rounded_rectangle((x+19,198,x+51,226),radius=4,fill=(50,130,225) if i==0 else (240,190,25),outline=white)
        d.text((x+35-tlen(d,glyph,bold)//2,205),glyph,font=bold,fill=white)
        d.text((x+36-tlen(d,label,regular)//2,237),label,font=regular,fill='black')
    d.rectangle((8,266,231,285),fill=rgb565(0x4BE5))
    d.text((13,272),'Hold MENU: tasks    Hold OPT: settings',font=small,fill=white)
    d.rectangle((0,298,239,319),fill=rgb565(0xB6EE));d.line((0,298,239,298),fill=white)
    for x in (80,160):d.line((x,301,x,316),fill=(148,182,88))
    for s,x,align in (('Menu',4,'l'),('Open',120,'c'),('Quick',236,'r')):
        if align=='c':x-=tlen(d,s,regular)//2
        elif align=='r':x-=tlen(d,s,regular)
        d.text((x,303),s,font=regular,fill=(26,68,22))
    target=OUT/filename;im.save(target);print(target)
    return im
connected=render('WiFi Home-WLAN  -38dBm',True,'v17_idle_wifi_online_240x320.png')
searching=render('WiFi: scanning saved...',False,'v17_idle_wifi_scanning_240x320.png')
sheet=Image.new('RGB',(480,320),'white');sheet.paste(searching,(0,0));sheet.paste(connected,(240,0))
sheet.save(OUT/'v17_idle_wifi_comparison_480x320.png')
