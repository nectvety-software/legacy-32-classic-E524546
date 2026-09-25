#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'preview'
OUT.mkdir(exist_ok=True)
W,H=240,320
TITLE_H=30; CONTENT_TOP=31; SOFT_TOP=296; SOFT_H=24; ROW_H=42; ICON=28

def rgb565(v):
    return (((v>>11)&31)*255//31, ((v>>5)&63)*255//63, (v&31)*255//31)
C={
    'bg':rgb565(0x2104),'panel':rgb565(0x2945),'selected':rgb565(0x4208),
    'text':rgb565(0xEF7D),'dim':rgb565(0xAD55),'chrome':rgb565(0xD69A),
    'chromeText':(10,10,10),'accent':rgb565(0x05FF),'border':rgb565(0xFFFF),
    'edge':rgb565(0x8410),'track':rgb565(0x4208),'thumb':rgb565(0xC618)
}
try:
    F1=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf',9)
    F2=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf',12)
    F2B=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',14)
    FCLOCK=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',28)
except Exception:
    F1=F2=F2B=FCLOCK=ImageFont.load_default()

def txt(d,xy,s,fill=None,font=None,anchor=None):
    d.text(xy,s,fill=fill or C['text'],font=font or F1,anchor=anchor)

def icon(d,x,y,kind,bg):
    d.rectangle((x,y,x+ICON-1,y+ICON-1),fill=bg)
    ox=x+2; oy=y+2
    if kind=='Wi':
        col=(40,155,255)
        d.arc((ox+2,oy+2,ox+22,oy+22),210,330,fill=col,width=2)
        d.arc((ox+6,oy+6,ox+18,oy+18),210,330,fill=col,width=2)
        d.ellipse((ox+10,oy+17,ox+14,oy+21),fill=(255,255,255))
    elif kind in ('BLE','BT'):
        col=(20,215,255); d.rectangle((ox+2,oy+1,ox+21,oy+22),fill=(20,40,130))
        d.line((ox+12,oy+3,ox+12,oy+21),fill=col,width=2)
        d.line((ox+12,oy+3,ox+19,oy+9,ox+7,oy+17),fill=col,width=2)
        d.line((ox+7,oy+7,ox+19,oy+15,ox+12,oy+21),fill=col,width=2)
    elif kind=='Mus':
        col=(255,220,20); d.line((ox+14,oy+3,ox+14,oy+17),fill=col,width=3); d.line((ox+14,oy+3,ox+22,oy+3),fill=col,width=3)
        d.ellipse((ox+6,oy+15,ox+13,oy+22),fill=col); d.ellipse((ox+16,oy+13,ox+23,oy+20),fill=col)
    elif kind=='Dir':
        col=(245,182,45); d.rectangle((ox+2,oy+7,ox+22,oy+21),fill=col,outline=(30,30,30)); d.rectangle((ox+4,oy+4,ox+12,oy+9),fill=col)
    elif kind in ('Col','Pic'):
        d.rectangle((ox+2,oy+3,ox+22,oy+21),fill=(112,60,126),outline=(240,240,240)); d.ellipse((ox+16,oy+6,ox+20,oy+10),fill=(255,230,0)); d.polygon([(ox+4,oy+19),(ox+10,oy+11),(ox+15,oy+19)],fill=(20,180,80)); d.polygon([(ox+11,oy+19),(ox+17,oy+12),(ox+22,oy+19)],fill=(20,170,220))
    elif kind=='Set':
        col=(45,100,245); d.ellipse((ox+4,oy+4,ox+20,oy+20),fill=col); d.rectangle((ox+10,oy+1,ox+14,oy+23),fill=col); d.rectangle((ox+1,oy+10,ox+23,oy+14),fill=col); d.ellipse((ox+9,oy+9,ox+15,oy+15),fill=bg)
    elif kind=='App':
        cols=[(35,75,225),(220,50,45),(45,190,75),(230,205,30)]
        for c,(dx,dy) in zip(cols,[(2,2),(13,2),(2,13),(13,13)]): d.rectangle((ox+dx,oy+dy,ox+dx+9,oy+dy+9),fill=c)
    elif kind=='Clk':
        col=(30,220,70); d.ellipse((ox+3,oy+3,ox+21,oy+21),outline=col,width=2); d.line((ox+12,oy+12,ox+12,oy+6),fill=col,width=2); d.line((ox+12,oy+12,ox+18,oy+12),fill=col,width=2)
    elif kind=='Sys':
        d.rectangle((ox+2,oy+4,ox+22,oy+18),fill=(105,105,105),outline=(245,245,245)); d.line((ox+5,oy+9,ox+19,oy+9),fill=(50,100,245),width=2); d.line((ox+5,oy+13,ox+15,oy+13),fill=(35,205,75),width=2); d.rectangle((ox+8,oy+20,ox+17,oy+22),fill=(230,230,230))
    elif kind=='Bell':
        d.ellipse((ox+6,oy+4,ox+18,oy+16),fill=(240,190,20)); d.rectangle((ox+6,oy+10,ox+18,oy+18),fill=(240,190,20)); d.ellipse((ox+10,oy+19,ox+14,oy+23),fill=(255,230,100))
    elif kind=='Lock':
        d.arc((ox+7,oy+4,ox+18,oy+15),180,360,fill=(220,220,220),width=2); d.rectangle((ox+6,oy+11,ox+19,oy+22),fill=(65,65,70),outline=(220,220,220)); d.ellipse((ox+11,oy+15,ox+14,oy+18),fill=(255,255,255))
    else:
        d.rectangle((ox+5,oy+2,ox+20,oy+22),fill=(242,242,242),outline=(20,20,20)); d.line((ox+8,oy+10,ox+17,oy+10),fill=(80,80,80)); d.line((ox+8,oy+14,ox+17,oy+14),fill=(80,80,80))

def chrome(d,title,clock='05:55',wifi=True,ble=False,sd=True):
    d.rectangle((0,0,W-1,TITLE_H-1),fill=C['chrome'])
    d.line((0,TITLE_H-1,W-1,TITLE_H-1),fill=C['edge'])
    d.line((0,TITLE_H,W-1,TITLE_H),fill=C['border'])
    d.line((6,8,12,14),fill=C['chromeText']); d.line((12,8,6,14),fill=C['chromeText'])
    txt(d,(18,2),title[:15],fill=C['chromeText'],font=F2B)
    sx=178
    if wifi:
        for i,h in enumerate((2,4,6,8)): d.rectangle((sx+i*3,11-h,sx+i*3+1,11),fill=C['chromeText'])
        sx+=14
    if ble:
        txt(d,(sx,2),'B',fill=C['chromeText'],font=F2); sx+=11
    if sd:
        d.rectangle((sx,3,sx+7,12),outline=C['chromeText']); d.rectangle((sx+2,4,sx+5,6),fill=C['chromeText'])
    d.rectangle((225,3,233,10),outline=C['chromeText']); d.rectangle((234,5,235,8),fill=C['chromeText']); d.rectangle((227,5,231,8),fill=C['chromeText'])
    txt(d,(236,17),clock,fill=C['chromeText'],font=F1,anchor='ra')

def soft(d,left='',center='',right=''):
    d.rectangle((0,SOFT_TOP,W-1,H-1),fill=C['chrome'])
    d.line((0,SOFT_TOP,W-1,SOFT_TOP),fill=C['border']); d.line((0,SOFT_TOP+1,W-1,SOFT_TOP+1),fill=C['edge'])
    txt(d,(4,300),left,fill=C['chromeText'],font=F2)
    txt(d,(120,300),center,fill=C['chromeText'],font=F2,anchor='ma')
    txt(d,(236,300),right,fill=C['chromeText'],font=F2,anchor='ra')

def row(d,row_i,kind,title,sub,selected=False):
    y=CONTENT_TOP+1+row_i*ROW_H; bg=C['selected'] if selected else C['bg']
    d.rectangle((2,y,234,y+ROW_H-2),fill=bg)
    if selected: d.rectangle((3,y+1,233,y+ROW_H-3),outline=C['border'])
    icon(d,8,y+6,kind,bg)
    txt(d,(42,y+5),title[:23],font=F2B)
    txt(d,(42,y+24),sub[:34],fill=C['dim'],font=F1)

def scrollbar(d,total,visible,offset,y0=34,y1=291):
    x=236; d.rectangle((x,y0,x+2,y1),fill=C['track'])
    if total<=visible: return
    h=max(14,int((y1-y0+1)*visible/total)); travel=(y1-y0+1)-h; top=y0+int(travel*offset/max(1,total-visible))
    d.rectangle((x,top,x+2,top+h),fill=C['thumb'])

def launcher():
    im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Menu')
    items=[('WiFi','Wi'),('Bluetooth','BLE'),('Music','Mus'),('File mgr','Dir'),('Collection','Col'),('Settings','Set'),('Apps','App'),('Clock','Clk'),('System','Sys')]
    for i,(name,ic) in enumerate(items):
        col=i%3; rr=i//3; x=col*80+2; y=CONTENT_TOP+rr*67+2; bg=C['selected'] if i==0 else C['panel']
        d.rectangle((x,y,x+75,y+61),fill=bg,outline=C['border'] if i==0 else C['dim'])
        icon(d,x+24,y+5,ic,bg); txt(d,(x+38,y+39),name,font=F1,anchor='ma')
    d.rectangle((5,237,234,290),fill=C['panel'],outline=C['dim']); txt(d,(12,244),'WiFi',font=F2B); txt(d,(12,264),'Scan and connect',fill=C['dim'],font=F1)
    soft(d,'Options','Open',''); return im

def settings():
    im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Settings')
    data=[('Th','Theme','Classic beige'),('Br','Backlight','90%'),('Mus','Audio volume','75%'),('Clk','Clock format','24-hour'),('Wi','WiFi auto reconnect','On'),('Lock','Auto keypad lock','60 sec')]
    for i,a in enumerate(data): row(d,i,*a,selected=(i==2))
    scrollbar(d,7,6,0); soft(d,'Options','Change','Back'); return im

def wifi():
    im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'WiFi',wifi=True)
    data=[('Wi','Home_2.4G','-42 dBm  Secured  Saved'),('Wi','Cafe_Free','-61 dBm  Open'),('Wi','Workshop','-68 dBm  Secured  Saved'),('Wi','ESP32_AP','-73 dBm  Secured'),('Wi','Guest','-79 dBm  Secured'),('Wi','Phone hotspot','-82 dBm  Secured')]
    for i,a in enumerate(data): row(d,i,*a,selected=(i==1))
    scrollbar(d,11,6,0); soft(d,'Options','Connect','Back'); return im

def idle():
    im=Image.new('RGB',(W,H),rgb565(0x0010)); d=ImageDraw.Draw(im)
    # low-cost banded wallpaper matching primitive firmware graphics
    bands=[0x0010,0x0015,0x0218,0x031A,0x041C,0x051E]
    for i,b in enumerate(bands): d.rectangle((0,i*46,W-1,min(295,(i+1)*46-1)),fill=rgb565(b))
    d.rectangle((0,0,W-1,20),fill=C['chrome']); txt(d,(6,5),'General',fill=C['chromeText'],font=F2)
    # status symbols right
    d.rectangle((225,4,233,11),outline=C['chromeText']); d.rectangle((234,6,235,9),fill=C['chromeText'])
    txt(d,(120,52),'05:55',font=FCLOCK,anchor='ma'); txt(d,(120,91),'Thu 24 Sep',font=F2B,anchor='ma')
    d.rectangle((12,116,227,172),fill=rgb565(0x18C3),outline=C['dim'])
    txt(d,(20,124),'Home_2.4G',font=F2B); txt(d,(20,145),'2 unread  |  SD ready',fill=C['dim'],font=F1)
    tiles=[('WiFi','Wi'),('Music','Mus'),('Files','Dir')]
    for i,(name,ic) in enumerate(tiles):
        x=12+i*75; y=193; bg=C['selected'] if i==0 else C['panel']; d.rectangle((x,y,x+63,y+62),fill=bg,outline=C['border'] if i==0 else C['dim']); icon(d,x+18,y+7,ic,bg); txt(d,(x+31,y+43),name,font=F1,anchor='ma')
    txt(d,(10,266),'D-pad shortcuts  |  OPTION quick panel',fill=C['dim'],font=F1)
    soft(d,'Menu','Open','Quick'); return im

imgs={'idle_home':idle(),'launcher':launcher(),'wifi':wifi(),'settings':settings()}
for name,im in imgs.items():
    im.save(OUT/f'v061_{name}_240x320.png')
    im.resize((W*3,H*3),Image.Resampling.NEAREST).save(OUT/f'v061_{name}_3x.png')

sheet=Image.new('RGB',(W*2,H*2),(16,16,16))
for i,name in enumerate(['idle_home','launcher','wifi','settings']): sheet.paste(imgs[name],((i%2)*W,(i//2)*H))
sheet.save(OUT/'v061_contact_sheet_480x640.png')
sheet.resize((W*4,H*4),Image.Resampling.NEAREST).save(OUT/'v061_contact_sheet_2x.png')
print(OUT/'v061_contact_sheet_2x.png')
