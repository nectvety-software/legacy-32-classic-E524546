#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'preview'
OUT.mkdir(exist_ok=True)
W,H=240,320

def rgb565(v):
    r=((v>>11)&31)*255//31; g=((v>>5)&63)*255//63; b=(v&31)*255//31
    return (r,g,b)
C={
'bg':rgb565(0x2104),'panel':rgb565(0x2945),'selected':rgb565(0x4208),'text':rgb565(0xEF7D),
'dim':rgb565(0xAD55),'chrome':rgb565(0xD69A),'chromeText':(0,0,0),'accent':rgb565(0x05FF),
'border':(255,255,255),'wall1':rgb565(0x0010),'wall2':rgb565(0x0218),'wall3':rgb565(0x031A),'wall4':rgb565(0x041C),
}
try:
    FONT=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',10)
    FONT_B=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',12)
    FONT_L=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',26)
except:
    FONT=FONT_B=FONT_L=ImageFont.load_default()

def text(draw,xy,s,fill=None,font=None,anchor=None):
    draw.text(xy,s,fill=fill or C['text'],font=font or FONT,anchor=anchor)

def icon(draw,x,y,kind,bg=None):
    bg=bg or C['panel']
    draw.rectangle((x,y,x+23,y+23),fill=bg)
    if kind=='Wi':
        for r in (9,6): draw.arc((x+12-r,y+12-r,x+12+r,y+12+r),200,340,fill=(30,150,255),width=2)
        draw.ellipse((x+10,y+17,x+14,y+21),fill=(255,255,255))
    elif kind=='BLE':
        col=(0,220,255); draw.line((x+12,y+3,x+12,y+20),fill=col,width=2)
        draw.line((x+12,y+3,x+18,y+9,x+7,y+17),fill=col,width=2)
        draw.line((x+7,y+7,x+18,y+15,x+12,y+20),fill=col,width=2)
    elif kind=='Mus':
        col=(255,220,0); draw.line((x+14,y+4,x+14,y+17),fill=col,width=3); draw.line((x+14,y+4,x+21,y+4),fill=col,width=3)
        draw.ellipse((x+6,y+15,x+13,y+22),fill=col); draw.ellipse((x+15,y+13,x+22,y+20),fill=col)
    elif kind=='Dir':
        col=(245,180,40); draw.rectangle((x+2,y+7,x+21,y+20),fill=col,outline=(0,0,0)); draw.rectangle((x+4,y+4,x+11,y+9),fill=col)
    elif kind=='Col':
        draw.rectangle((x+2,y+3,x+21,y+20),fill=(120,60,120),outline=(255,255,255)); draw.ellipse((x+16,y+6,x+19,y+9),fill=(255,230,0)); draw.polygon([(x+4,y+18),(x+10,y+10),(x+14,y+18)],fill=(0,180,80)); draw.polygon([(x+10,y+18),(x+16,y+12),(x+21,y+18)],fill=(0,180,220))
    elif kind=='Set':
        col=(50,110,255); draw.ellipse((x+4,y+4,x+20,y+20),fill=col); draw.ellipse((x+9,y+9,x+15,y+15),fill=bg)
    elif kind=='App':
        cols=[(40,80,230),(220,40,40),(40,200,80),(230,210,30)]
        for i,(dx,dy) in enumerate(((2,2),(13,2),(2,13),(13,13))): draw.rectangle((x+dx,y+dy,x+dx+8,y+dy+8),fill=cols[i])
    elif kind=='Clk':
        col=(30,220,70); draw.ellipse((x+3,y+3,x+21,y+21),outline=col,width=2); draw.line((x+12,y+12,x+12,y+6),fill=col,width=2); draw.line((x+12,y+12,x+18,y+12),fill=col,width=2)
    elif kind=='Sys':
        draw.rectangle((x+2,y+4,x+21,y+18),fill=(110,110,110),outline=(255,255,255)); draw.line((x+5,y+9,x+18,y+9),fill=(40,90,240),width=2); draw.line((x+5,y+13,x+15,y+13),fill=(30,210,70),width=2)
    elif kind=='Bell':
        draw.ellipse((x+6,y+4,x+18,y+16),fill=(240,190,20)); draw.rectangle((x+6,y+10,x+18,y+18),fill=(240,190,20)); draw.ellipse((x+10,y+18,x+14,y+22),fill=(255,220,80))
    else:
        draw.rectangle((x+4,y+3,x+19,y+21),fill=(245,245,245),outline=(20,20,20)); draw.line((x+7,y+10,x+17,y+10),fill=(70,70,70)); draw.line((x+7,y+14,x+17,y+14),fill=(70,70,70))

def chrome(draw,title,clock='05:55',wifi=True,ble=True,sd=True):
    draw.rectangle((0,0,W,31),fill=C['chrome']); draw.line((0,32,W,32),fill=C['border'])
    text(draw,(5,6),'x',fill=C['chromeText'],font=FONT_B); text(draw,(20,5),title[:16],fill=C['chromeText'],font=FONT_B)
    x=176
    if wifi:
        for i,h in enumerate((3,5,7,9)): draw.rectangle((x+i*3,10-h,x+i*3+1,10),fill=C['chromeText'])
        x+=14
    if ble: text(draw,(x,4),'B',fill=C['chromeText'],font=FONT_B); x+=10
    if sd: text(draw,(x,4),'S',fill=C['chromeText'],font=FONT_B)
    draw.rectangle((224,3,234,11),outline=C['chromeText']); draw.rectangle((234,6,236,9),fill=C['chromeText']); draw.rectangle((226,5,231,9),fill=C['chromeText'])
    text(draw,(204,18),clock,fill=C['chromeText'],font=FONT,anchor='ma')

def soft(draw,left='',center='',right=''):
    draw.rectangle((0,292,W,319),fill=C['chrome']); draw.line((0,292,W,292),fill=C['border'])
    text(draw,(4,300),left,fill=C['chromeText'],font=FONT_B); text(draw,(120,300),center,fill=C['chromeText'],font=FONT_B,anchor='ma'); text(draw,(236,300),right,fill=C['chromeText'],font=FONT_B,anchor='ra')

def idle():
    im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im)
    bands=[0x0010,0x0015,0x0218,0x031A,0x041C,0x051E]
    for i,b in enumerate(bands): d.rectangle((0,i*44,W,(i+1)*44),fill=rgb565(b))
    d.rectangle((0,245,W,291),fill=rgb565(0x1082))
    for x in range(0,W,18):
        hh=10+((x//18)%4)*5; d.rectangle((x,245-hh,x+12,245),fill=rgb565(0x18C3))
    d.rectangle((0,0,W,21),fill=C['chrome']); text(d,(6,7),'General',fill=C['chromeText'])
    text(d,(120,58),'05:55',font=FONT_L,anchor='ma'); text(d,(120,96),'Thu 24 Sep',font=FONT_B,anchor='ma')
    text(d,(12,128),'WiFi: Home_2.4G'); text(d,(12,145),'microSD: ready'); text(d,(12,162),'Notifications: 2 unread')
    labels=[('WiFi','Wi'),('Music','Mus'),('Files','Dir')]; xs=[13,88,163]
    for i,(lab,ic) in enumerate(labels):
        bg=C['selected'] if i==0 else C['panel']; d.rectangle((xs[i],188,xs[i]+63,249),fill=bg,outline=C['border'] if i==0 else C['dim']); icon(d,xs[i]+20,196,ic,bg); text(d,(xs[i]+32,234),lab,anchor='ma')
    text(d,(8,253),'OPTION Quick | A/B Lock',font=FONT); text(d,(8,266),'Hold START Music | SEL WiFi',font=FONT); text(d,(8,279),'Hold MENU Tasks | OPT Settings',font=FONT)
    soft(d,'Menu','Open','Quick'); return im

def launcher():
    im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Menu')
    items=[('WiFi','Wi'),('Bluetooth','BLE'),('Music','Mus'),('File mgr','Dir'),('Collection','Col'),('Settings','Set'),('Apps','App'),('Clock','Clk'),('System','Sys')]
    for i,(lab,ic) in enumerate(items):
        col=i%3; row=i//3; x=2+col*78; y=38+row*67; bg=C['selected'] if i==0 else C['panel']; d.rectangle((x,y,x+74,y+61),fill=bg,outline=C['border'] if i==0 else C['dim']); icon(d,x+25,y+6,ic,bg); text(d,(x+37,y+38),lab,font=FONT,anchor='ma')
    d.rectangle((5,244,234,286),fill=C['panel'],outline=C['dim']); text(d,(12,251),'WiFi',font=FONT_B); text(d,(12,267),'Scan and connect',fill=C['dim'])
    soft(d,'Options','Open',''); return im

def tasks():
    im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Open applications')
    rows=[('Music','Mus','Most recently used'),('File manager','Dir','Suspended UI state'),('WiFi','Wi','Suspended UI state'),('Settings','Set','Suspended UI state'),('Notes','Doc','Suspended UI state')]
    y=36
    for i,(name,ic,sub) in enumerate(rows):
        yy=y+i*40; bg=C['selected'] if i==0 else C['panel']; d.rectangle((4,yy,232,yy+37),fill=bg,outline=C['border'] if i==0 else C['bg']); icon(d,10,yy+6,ic,bg); text(d,(42,yy+7),name,font=FONT_B); text(d,(42,yy+22),sub,fill=C['dim'])
    d.rectangle((235,36,238,288),fill=(90,90,90)); d.rectangle((235,36,238,126),fill=(240,240,240))
    text(d,(12,248),'Long MENU opens this screen.',fill=C['dim']); text(d,(12,264),'Switch resumes UI state.',fill=C['dim'])
    soft(d,'Options','Switch','Back'); return im

def wifi():
    im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'WiFi')
    rows=[('Home_2.4G','-42 dBm  Secured  Saved'),('Cafe_Free','-61 dBm  Open'),('Workshop','-68 dBm  Secured  Saved'),('ESP32_AP','-73 dBm  Secured'),('Guest','-79 dBm  Secured')]
    for i,(name,sub) in enumerate(rows):
        yy=36+i*40; bg=C['selected'] if i==0 else C['panel']; d.rectangle((4,yy,232,yy+37),fill=bg); icon(d,10,yy+6,'Wi',bg); text(d,(42,yy+6),name,font=FONT_B); text(d,(42,yy+22),sub,fill=C['dim'])
    text(d,(10,250),'Saved profiles: 2 / 5',fill=C['dim']); text(d,(10,266),'Options > Forget saved',fill=C['dim'])
    soft(d,'Options','Connect','Back'); return im

imgs={'idle_home':idle(),'launcher':launcher(),'task_switcher':tasks(),'wifi_profiles':wifi()}
for name,im in imgs.items():
    im.save(OUT/f'v06_{name}_240x320.png')
    im.resize((W*3,H*3),Image.Resampling.NEAREST).save(OUT/f'v06_{name}_3x.png')

sheet=Image.new('RGB',(W*2,H*2),(20,20,20))
for idx,name in enumerate(['idle_home','launcher','task_switcher','wifi_profiles']):
    sheet.paste(imgs[name],((idx%2)*W,(idx//2)*H))
sheet.save(OUT/'v06_contact_sheet_480x640.png')
sheet.resize((W*4,H*4),Image.Resampling.NEAREST).save(OUT/'v06_contact_sheet_2x.png')
print(OUT/'v06_contact_sheet_2x.png')
