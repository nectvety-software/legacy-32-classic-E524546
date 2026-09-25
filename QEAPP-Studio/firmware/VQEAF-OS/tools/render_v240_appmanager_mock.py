#!/usr/bin/env python3
"""Illustrative 240x320 App Manager mock, NOT a capture of C++ firmware."""
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
R=Path(__file__).resolve().parent.parent
OUT=R/'preview'
B='#8cc720';TOP='#2c6c20';WHITE='#f2ffc2';PALE='#dff09e';FOCUS='#fcfff5';TEXT='#131e0c'
try: FONT=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf',10)
except OSError: FONT=ImageFont.load_default()
try: BOLD=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf',11)
except OSError: BOLD=FONT

def text(d,x,y,value,color=TEXT,bold=False):
    d.text((x,y),value,font=BOLD if bold else FONT,fill=color)

def base(title,keys=('Options','Open','Back')):
    im=Image.new('RGB',(240,320),B);d=ImageDraw.Draw(im)
    d.rectangle((0,0,239,23),fill=TOP)
    text(d,5,6,title,'white',True)
    text(d,206,6,'Wi',WHITE)
    d.rectangle((0,23,239,26),fill='#bedc65')
    d.rectangle((0,298,239,319),fill=PALE)
    d.line((0,297,239,297),fill=TOP,width=1)
    for x in (79,159):d.line((x,298,x,319),fill='#a1c052')
    text(d,4,303,keys[0],bold=True);text(d,92,303,keys[1],bold=True);text(d,189,303,keys[2],bold=True)
    return im,d

screens=[]
im,d=base('App inbox',('Options','Details','Back'))
text(d,7,32,'Inbox    |    Installed',bold=True)
d.line((8,49,231,49),fill=TOP)
try:
   menu=Image.open(OUT/'v235_menu_240x320.png').convert('RGB')
   icon=menu.crop((99,229,135,265))
   im.paste(icon,(12,63))
except (OSError,ValueError):d.rectangle((12,63,47,98),fill='#f8b12a')
d.rectangle((7,58,232,110),outline=FOCUS,width=2)
text(d,57,65,'Welcome',bold=True);text(d,57,85,'v1.1.0  signed text app')
text(d,10,132,'Installer reads signed QEAPP/2')
text(d,10,150,'packages from SD/App inbox')
text(d,10,280,'12 entries max  |  SD online')
screens.append(('Inbox',im))

im,d=base('Signed app update',('Options','Update','Back'))
d.rectangle((9,45,230,231),fill=PALE)
text(d,17,54,'Welcome',bold=True)
text(d,17,79,'Current: 1.0.0')
text(d,17,96,'Signed:  1.1.0')
d.line((17,118,221,118),fill=TOP)
text(d,17,131,'Publisher signature: PASS')
text(d,17,153,'Type: bundled text')
text(d,17,169,'Permission: text read')
text(d,17,205,'Data slots are preserved')
text(d,11,249,'START: read and confirm update')
screens.append(('Update',im))

im,d=base('App installer',('','',''))
d.rectangle((9,61,230,221),fill=PALE)
text(d,17,75,'Updating Welcome',bold=True)
text(d,17,110,'Copying signed app',bold=True)
d.rectangle((17,140,219,157),outline=TOP,width=1)
d.rectangle((20,143,20+int(196*.54),154),fill='#286c27')
text(d,17,165,'54%')
text(d,17,191,'Do not remove the microSD')
screens.append(('Progress',im))

im,d=base('Install recovery',('Options','Rescan','Back'))
d.rectangle((9,46,230,225),fill=PALE)
text(d,16,61,'Verified packages only',bold=True)
text(d,16,94,'Restored: 1')
text(d,16,113,'Finalized: 1')
text(d,16,132,'Blocked:  0')
text(d,16,151,'Stale stages discarded: 1')
d.line((14,176,224,176),fill=TOP)
text(d,16,192,'Unknown user files: NEVER')
text(d,16,206,'deleted automatically.')
screens.append(('Recovery',im))
for name,im in screens:im.save(OUT/f'v240_app_manager_{name.lower()}_240x320.png')
canvas=Image.new('RGB',(2*260,2*350),'#253127');draw=ImageDraw.Draw(canvas)
for i,(name,im) in enumerate(screens):
 x=10+(i%2)*260;y=16+(i//2)*350
 draw.text((x,y-14),name,font=BOLD,fill='white')
 canvas.paste(im,(x,y))
canvas.save(OUT/'v240_app_manager_illustrative_layouts.png')
print('Generated 4 native-sized UI MOCK images, not runtime framebuffer captures.')
