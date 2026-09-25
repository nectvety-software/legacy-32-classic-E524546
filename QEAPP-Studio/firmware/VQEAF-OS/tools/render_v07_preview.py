#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'preview'; OUT.mkdir(exist_ok=True)
W,H=240,320; TITLE_H=30; CONTENT_TOP=31; SOFT_TOP=296; SOFT_H=24; ROW_H=42; ICON=28

def rgb565(v): return (((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31)
C={'bg':rgb565(0x2104),'panel':rgb565(0x2945),'selected':rgb565(0x4208),'text':rgb565(0xEF7D),'dim':rgb565(0xAD55),'chrome':rgb565(0xD69A),'chromeText':(10,10,10),'accent':rgb565(0x05FF),'border':rgb565(0xFFFF),'edge':rgb565(0x8410),'track':rgb565(0x4208),'thumb':rgb565(0xC618),'blue':(25,120,235),'green':(40,205,90)}
try:
 F1=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf',9); F2=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf',12); F2B=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',13); FB=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',16); FC=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',28)
except: F1=F2=F2B=FB=FC=ImageFont.load_default()

def txt(d,xy,s,fill=None,font=None,anchor=None): d.text(xy,s,fill=fill or C['text'],font=font or F1,anchor=anchor)

def wifi_icon(d,x,y,ink):
 for i,h in enumerate((2,4,6,8)): d.rectangle((x+i*3,y+9-h,x+i*3+1,y+8),fill=ink)
def battery_icon(d,x,y,ink):
 d.rectangle((x,y+1,x+8,y+7),outline=ink); d.rectangle((x+9,y+3,x+10,y+5),fill=ink); d.rectangle((x+2,y+3,x+6,y+5),fill=ink)

def chrome(d,title,clock='05:55',wifi=True):
 d.rectangle((0,0,W-1,TITLE_H-1),fill=C['chrome']); d.line((0,TITLE_H-1,W-1,TITLE_H-1),fill=C['edge']); d.line((0,TITLE_H,W-1,TITLE_H),fill=C['border'])
 d.line((6,8,12,14),fill=C['chromeText']); d.line((12,8,6,14),fill=C['chromeText']); txt(d,(18,2),title[:15],fill=C['chromeText'],font=F2B)
 # Deliberately only WiFi + battery. No SIM/BLE/SD status glyphs.
 if wifi: wifi_icon(d,193,3,C['chromeText'])
 battery_icon(d,225,2,C['chromeText']); txt(d,(236,18),clock,fill=C['chromeText'],font=F1,anchor='ra')

def soft(d,left='',center='',right=''):
 d.rectangle((0,SOFT_TOP,W-1,H-1),fill=C['chrome']); d.line((0,SOFT_TOP,W-1,SOFT_TOP),fill=C['border']); d.line((0,SOFT_TOP+1,W-1,SOFT_TOP+1),fill=C['edge'])
 txt(d,(4,301),left,fill=C['chromeText'],font=F2); txt(d,(120,301),center,fill=C['chromeText'],font=F2,anchor='ma'); txt(d,(236,301),right,fill=C['chromeText'],font=F2,anchor='ra')

def icon(d,x,y,kind,bg=None):
 bg=bg or C['panel']; d.rectangle((x,y,x+ICON-1,y+ICON-1),fill=bg); ox=x+2; oy=y+2
 if kind=='Wi':
  col=(40,155,255); d.arc((ox+2,oy+2,ox+22,oy+22),210,330,fill=col,width=2); d.arc((ox+6,oy+6,ox+18,oy+18),210,330,fill=col,width=2); d.ellipse((ox+10,oy+17,ox+14,oy+21),fill=(255,255,255))
 elif kind=='BLE':
  col=(20,215,255); d.rectangle((ox+2,oy+1,ox+21,oy+22),fill=(20,40,130)); d.line((ox+12,oy+3,ox+12,oy+21),fill=col,width=2); d.line((ox+12,oy+3,ox+19,oy+9,ox+7,oy+17),fill=col,width=2); d.line((ox+7,oy+7,ox+19,oy+15,ox+12,oy+21),fill=col,width=2)
 elif kind=='Mus':
  col=(255,220,20); d.line((ox+14,oy+3,ox+14,oy+17),fill=col,width=3); d.line((ox+14,oy+3,ox+22,oy+3),fill=col,width=3); d.ellipse((ox+6,oy+15,ox+13,oy+22),fill=col); d.ellipse((ox+16,oy+13,ox+23,oy+20),fill=col)
 elif kind=='Dir':
  col=(245,182,45); d.rectangle((ox+2,oy+7,ox+22,oy+21),fill=col,outline=(30,30,30)); d.rectangle((ox+4,oy+4,ox+12,oy+9),fill=col)
 elif kind in ('Pic','Col'):
  d.rectangle((ox+2,oy+3,ox+22,oy+21),fill=(112,60,126),outline=(240,240,240)); d.ellipse((ox+16,oy+6,ox+20,oy+10),fill=(255,230,0)); d.polygon([(ox+4,oy+19),(ox+10,oy+11),(ox+15,oy+19)],fill=(20,180,80)); d.polygon([(ox+11,oy+19),(ox+17,oy+12),(ox+22,oy+19)],fill=(20,170,220))
 elif kind=='Web':
  col=(40,135,245); d.ellipse((ox+2,oy+2,ox+22,oy+22),outline=col,width=2); d.ellipse((ox+7,oy+2,ox+17,oy+22),outline=col,width=1); d.line((ox+3,oy+12,ox+21,oy+12),fill=col,width=1); d.line((ox+12,oy+3,ox+12,oy+21),fill=col,width=1)
 elif kind=='App':
  for c,(dx,dy) in zip([(35,75,225),(220,50,45),(45,190,75),(230,205,30)],[(2,2),(13,2),(2,13),(13,13)]): d.rectangle((ox+dx,oy+dy,ox+dx+9,oy+dy+9),fill=c)
 elif kind=='Set':
  col=(45,100,245); d.ellipse((ox+4,oy+4,ox+20,oy+20),fill=col); d.rectangle((ox+10,oy+1,ox+14,oy+23),fill=col); d.rectangle((ox+1,oy+10,ox+23,oy+14),fill=col); d.ellipse((ox+9,oy+9,ox+15,oy+15),fill=bg)
 elif kind=='Rec':
  col=(245,170,25); d.arc((ox+3,oy+3,ox+21,oy+21),40,320,fill=col,width=3); d.polygon([(ox+19,oy+3),(ox+23,oy+7),(ox+17,oy+8)],fill=col); txt(d,(ox+12,oy+12),'!',fill=(255,255,255),font=F2B,anchor='mm')
 elif kind=='Doc':
  d.rectangle((ox+5,oy+2,ox+20,oy+22),fill=(242,242,242),outline=(20,20,20)); d.line((ox+8,oy+9,ox+17,oy+9),fill=(70,70,70)); d.line((ox+8,oy+13,ox+17,oy+13),fill=(70,70,70)); d.line((ox+8,oy+17,ox+15,oy+17),fill=(70,70,70))
 else: d.rectangle((ox+4,oy+4,ox+20,oy+20),outline=C['text'])

def row(d,r,kind,title,sub,sel=False):
 y=CONTENT_TOP+1+r*ROW_H; bg=C['selected'] if sel else C['bg']; d.rectangle((2,y,234,y+ROW_H-2),fill=bg); 
 if sel: d.rectangle((3,y+1,233,y+ROW_H-3),outline=C['border'])
 icon(d,8,y+6,kind,bg); txt(d,(42,y+5),title[:24],font=F2B); txt(d,(42,y+24),sub[:35],fill=C['dim'],font=F1)

def scrollbar(d,total,vis,off,y0=34,y1=291):
 d.rectangle((236,y0,238,y1),fill=C['track']);
 if total<=vis:return
 h=max(14,int((y1-y0+1)*vis/total)); top=y0+int(((y1-y0+1)-h)*off/max(1,total-vis)); d.rectangle((236,top,238,top+h),fill=C['thumb'])

def launcher():
 im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Menu')
 items=[('WiFi','Wi'),('Bluetooth','BLE'),('Music','Mus'),('File mgr','Dir'),('Gallery','Pic'),('Web','Web'),('Apps','App'),('Settings','Set'),('Library','Col')]
 for i,(name,ic) in enumerate(items):
  col=i%3; rr=i//3; x=col*80+2; y=CONTENT_TOP+rr*67+2; bg=C['selected'] if i==5 else C['panel']; d.rectangle((x,y,x+75,y+61),fill=bg,outline=C['border'] if i==5 else C['dim']); icon(d,x+24,y+5,ic,bg); txt(d,(x+38,y+39),name,font=F1,anchor='ma')
 d.rectangle((5,237,234,290),fill=C['panel'],outline=C['dim']); txt(d,(12,244),'Web',font=F2B); txt(d,(12,264),'Qeafbrowser keypad web',fill=C['dim'],font=F1); soft(d,'Options','Open',''); return im

def opening():
 im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Opening')
 icon(d,106,78,'Web',C['panel']); txt(d,(120,123),'Dang mo ung dung',font=F2,anchor='ma'); txt(d,(120,149),'Qeafbrowser',font=FB,anchor='ma'); d.rectangle((45,184,194,193),outline=C['dim']); d.rectangle((47,186,163,191),fill=C['accent']); txt(d,(120,211),'Symbian S60 application startup',fill=C['dim'],font=F1,anchor='ma'); soft(d,'','',''); return im

def recovery():
 im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Safe mode')
 rows=[('Rec','Boot status','Task watchdog / crashes 2'),('Rec','Start normal mode','Disable Safe Mode and restart'),('Rec','Enable Safe Mode','Safe Mode is active'),('Rec','Clear recovery flags','Clear crash-loop markers'),('Rec','Restart device','Software restart')]
 for i,a in enumerate(rows): row(d,i,*a,sel=(i==2))
 txt(d,(10,251),'Audio / BLE / Browser / WiFi auto-connect',fill=C['dim'],font=F1); txt(d,(10,266),'disabled until normal mode.',fill=C['dim'],font=F1); soft(d,'','Select','Back'); return im

def gallery():
 im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Gallery')
 rows=[('Pic','photo_001.jpg','684 KB'),('Pic','wallpaper.png','128 KB'),('Pic','diagram.bmp','93 KB'),('Pic','nokia_ui.jpg','412 KB'),('Pic','cover.png','211 KB'),('Pic','pixel_art.bmp','64 KB')]
 for i,a in enumerate(rows): row(d,i,*a,sel=(i==1))
 scrollbar(d,9,6,0); soft(d,'Options','View','Back'); return im

def browser():
 im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Qeafbrowser')
 d.rectangle((5,36,234,59),fill=C['panel'],outline=C['dim']); txt(d,(9,44),'https://qeafivels.com/',fill=C['dim'],font=F1)
 lines=[('Qeafivels',False),('Legacy keypad projects',False),('Symbian / MRE / ESP32',False),('Projects',True),('Documentation',True),('Downloads',True),('A lightweight page rendered',False),('with fixed text/link pools.',False),('No JavaScript engine.',False),('Gallery and Text Viewer',False),('remain local OS apps.',False)]
 y=67
 for i,(s,link) in enumerate(lines):
  sel=(i==4); bg=C['selected'] if sel else C['bg']; d.rectangle((4,y-2,235,y+12),fill=bg); txt(d,(7,y),s,fill=C['blue'] if link else C['text'],font=F1); 
  if sel:d.rectangle((4,y-2,235,y+12),outline=C['border'])
  y+=16
 scrollbar(d,18,13,0,64,278); soft(d,'Options','Open','Back'); return im

def textviewer():
 im=Image.new('RGB',(W,H),C['bg']); d=ImageDraw.Draw(im); chrome(d,'Text viewer')
 d.rectangle((5,36,234,278),fill=C['panel'],outline=C['dim']); txt(d,(10,43),'README.md',font=F2B); y=65
 for line in ['# Symbian S3 OS v0.7','','Recovery / Safe Mode','Gallery / Text Viewer','Qeafbrowser integration','','This viewer streams bounded','pages from microSD and does','not load the entire document.','','RIGHT/DOWN: next page','LEFT/UP: previous page']:
  txt(d,(10,y),line,fill=C['text'] if not line.startswith('#') else C['accent'],font=F1); y+=16
 txt(d,(10,267),'Page 1',fill=C['dim'],font=F1); soft(d,'Options','Next','Back'); return im

imgs={'launcher':launcher(),'opening_qeafbrowser':opening(),'safe_mode_recovery':recovery(),'gallery':gallery(),'qeafbrowser':browser(),'text_viewer':textviewer()}
for name,im in imgs.items():
 im.save(OUT/f'v07_{name}_240x320.png'); im.resize((W*3,H*3),Image.Resampling.NEAREST).save(OUT/f'v07_{name}_3x.png')
sheet=Image.new('RGB',(W*3,H*2),(15,15,15))
for i,name in enumerate(imgs): sheet.paste(imgs[name],((i%3)*W,(i//3)*H))
sheet.save(OUT/'v07_contact_sheet_720x640.png'); sheet.resize((W*6,H*4),Image.Resampling.NEAREST).save(OUT/'v07_contact_sheet_2x.png')
print(OUT/'v07_contact_sheet_2x.png')
