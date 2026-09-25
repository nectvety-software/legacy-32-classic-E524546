#!/usr/bin/env python3
"""Draw *illustrative* 240x320 S60 UI references. Not a device screenshot."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'preview';OUT.mkdir(exist_ok=True)
W,H=240,320
F=Path('/usr/share/fonts/truetype/dejavu')
regular=ImageFont.truetype(str(F/'DejaVuSansCondensed.ttf'),10)
bold=ImageFont.truetype(str(F/'DejaVuSansCondensed-Bold.ttf'),12)
small=ImageFont.truetype(str(F/'DejaVuSansCondensed.ttf'),9)
white=(255,255,255);bg=(143,200,35);selected=(222,243,155);ink=(10,10,10);titleBg=(43,105,26)
def width(draw,text,font):
  b=draw.textbbox((0,0),text,font=font);return b[2]-b[0]
def shell(name,center,right):
  im=Image.new('RGB',(W,H),bg);d=ImageDraw.Draw(im)
  d.rectangle((0,0,239,26),fill=titleBg);d.line((0,25,239,25),fill=(83,151,62));d.line((0,26,239,26),fill=(170,218,93))
  d.text((4,4),'WiFi',font=bold,fill=white)
  clock='11:50';d.text(((240-width(d,clock,small))//2,10),clock,font=small,fill=white)
  wx,bx=206,223
  for i,h in enumerate((2,4,6,8)):d.rectangle((wx+i*3,17-h,wx+i*3+1,16),fill=white)
  d.rectangle((bx,8,bx+8,14),outline=white);d.rectangle((bx+9,10,bx+10,12),fill=white);d.rectangle((bx+2,10,bx+6,12),fill=white)
  d.rectangle((0,298,239,319),fill=(183,229,126));d.line((0,298,239,298),fill=white)
  for x in (80,160):d.line((x,301,x,316),fill=(139,183,87))
  d.text((4,303),name,font=bold,fill=(27,70,22))
  d.text(((240-width(d,center,bold))//2,303),center,font=bold,fill=(27,70,22))
  d.text((236-width(d,right,bold),303),right,font=bold,fill=(27,70,22))
  return im,d
im,d=shell('Options','Connect','Back')
rows=[('Home-WLAN','-38 dBm  Secured  Saved',True),('Qeafivels','-45 dBm  Secured',False),('IoT-Lab','-58 dBm  Secured',False),('Guest','-64 dBm  Open',False),('Office_2G','-71 dBm  Secured',False),('<hidden>','-79 dBm  Secured',False)]
for i,(name,sub,focus) in enumerate(rows):
  y=29+i*42
  if focus:d.rectangle((2,y,234,y+40),fill=selected,outline=white)
  else:d.rectangle((2,y,234,y+40),fill=bg)
  if focus:d.line((3,y+1,3,y+39),fill=titleBg)
  # lightweight WLAN icon, corresponds to WiFi `Wi` icon in firmware
  ix=16;iy=y+7
  for r in (9,6,3):d.arc((ix-r,iy-r,ix+r,iy+r),205,335,fill=(18,115,235),width=1)
  d.ellipse((ix-1,iy+6,ix+1,iy+8),fill=(18,115,235))
  d.text((47,y+3),name,font=bold,fill=ink)
  d.text((49,y+24),sub,font=small,fill=(41,77,42))
d.rectangle((236,34,237,281),fill=(208,236,158));d.rectangle((236,37,237,119),fill=white)
a=OUT/'v16_wifi_networks_240x320.png';im.save(a)
b,d=shell('','', 'Cancel')
d.text((12,50),'Connecting...',font=bold,fill=ink)
d.text((12,86),'Home-WLAN',font=regular,fill=ink)
d.text((12,106),'Please wait (up to 12s)',font=regular,fill=ink)
d.text((12,164),'Joining 4 / 12s',font=bold,fill=ink)
d.rectangle((12,189,218,199),outline=(56,87,51));d.rectangle((14,191,79,197),fill=(240,211,10))
c=OUT/'v16_wifi_connecting_240x320.png';b.save(c)
sheet=Image.new('RGB',(W*2,H),(255,255,255));sheet.paste(im,(0,0));sheet.paste(b,(W,0));s=OUT/'v16_wifi_preview_480x320.png';sheet.save(s)
print(a);print(c);print(s)
