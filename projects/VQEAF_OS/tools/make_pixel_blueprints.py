#!/usr/bin/env python3
"""Pixel-accurate schematic blueprints only, NOT emulator/hardware captures."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json,math
R=Path(__file__).resolve().parents[1]
J=json.loads((R/'docs/pixel_atlas.json').read_text('utf-8'))
DEST=R/'preview';DEST.mkdir(exist_ok=True)
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
f10=ImageFont.truetype(FONT,10);f11=ImageFont.truetype(FONT,11);f13=ImageFont.truetype(BOLD,13);f9=ImageFont.truetype(FONT,9)
pal=dict(bg='#8BCA20',chrome='#296D18',text='#12210A',panel='#B4DE73',selected='#DEF29C',selected_text='#12210A',dim='#2C4B1A',border='#FFFFFF',scrollbar='#49792D',footer='#B4DE73',terminal='#080A07',white='#FFFFFF')
def render(s):
 im=Image.new('RGB',(240,320),pal['bg']);d=ImageDraw.Draw(im)
 d.rectangle((0,0,239,26),fill=pal['chrome']);d.line((0,27,239,27),fill='#E1FFC2');
 d.text((4,4),'General' if s['id']=='home' else ('Menu' if s['id']=='menu' else s['title'].split('/')[0][:12]),font=f13,fill='white')
 d.text((111,8),'--:--',font=f9,fill='white')
 d.rectangle((218,8,229,16),outline='white');d.rectangle((230,10,232,14),fill='white')
 d.rectangle((0,298,239,319),fill=pal['footer']);d.line((0,298,239,298),fill='white')
 for x in (80,160):d.line((x,300,x,317),fill='#669A27')
 for text,x in [('Options',4),('Open',94),('Back',208)]: d.text((x,303),text,font=f10,fill=pal['text'])
 regs={r['id']:r for r in s['regions']}
 def box(id,fill=None,line=None,caption=None):
  if id not in regs:return
  r=regs[id];x,y,w,h=r['x'],r['y'],r['w'],r['h'];d.rectangle((x,y,x+w-1,y+h-1),fill=fill or pal['panel'],outline=line or pal['border'])
  if caption:d.text((x+4,y+4),caption,font=f10,fill=pal['text'])
 if s['id']=='home':
  box('clock_card',caption='General');d.text((78,69),'--:--',font=ImageFont.truetype(BOLD,23),fill=pal['text']);d.text((73,101),'Date not set',font=f10,fill=pal['text'])
  box('network_card');d.text((18,143),'WiFi: no saved networks',font=f10,fill=pal['text']);d.text((18,159),'0 unread notification(s)',font=f9,fill=pal['text'])
  for i,t in enumerate(['WiFi','Music','Files']):
   id='quick_'+t.lower();box(id,pal['selected'] if i==0 else pal['panel']);rr=regs[id];d.rectangle((rr['x']+25,rr['y']+12,rr['x']+47,rr['y']+34),fill=['#38D4FF','#E84587','#EBAA19'][i]);d.text((rr['x']+19,rr['y']+48),t,font=f10,fill=pal['text'])
  d.text((13,273),'Hold MENU: tasks  OPT: settings',font=f9,fill='white')
 elif s['id']=='menu':
  names=['WiFi','Bluetooth','Music','File mgr','Gallery','Internet','Shell','Recovery','Settings','Themes','Apps','Library']
  sym=['Wi','BT','Mu','Fi','Ga','Web','>_','!','*','Th','AP','Li']
  for i,name in enumerate(names):
   r=regs['cell_%02d'%i];x,y,w,h=r['x'],r['y'],r['w'],r['h'];bg=pal['selected'] if i==0 else ['#91CA2A','#82C223','#8BCA20','#82C326'][i//3];d.rectangle((x,y,x+w-1,y+h-1),fill=bg,outline=pal['border'] if i==0 else '#95D739',width=2 if i==0 else 1)
   ir=regs['icon_%02d'%i];ax,ay=ir['x'],ir['y'];d.rounded_rectangle((ax+5,ay+3,ax+30,ay+27),radius=2,fill=['#3EABF6','#212BC2','#E843B2','#EDB33A'][i%4],outline=pal['white']);d.text((ax+7,ay+6),sym[i],font=f10,fill='white');d.text((x+max(1,38-len(name)*3),y+45),name,font=f9,fill=pal['text'])
  r=regs['grid_scroll'];d.rectangle((r['x'],r['y'],r['x']+r['w']-1,r['y']+r['h']-1),fill=pal['scrollbar'])
 elif s['id']=='calculator':
  box('result');d.text((22,55),'0',font=f13,fill=pal['text'])
  for i,k in enumerate('789/456*123-C0=+'):
   rr=regs['key_'+str(i)];x,y,w,h=rr['x'],rr['y'],rr['w'],rr['h'];d.rectangle((x,y,x+w-1,y+h-1),fill=pal['selected'] if i==0 else pal['panel'],outline=pal['border']);d.text((x+19,y+12),k,font=f13,fill=pal['text'])
 elif s['id']=='stopwatch':
  box('face');d.text((59,89),'00:00.00',font=ImageFont.truetype(BOLD,19),fill=pal['text'])
  for n in ('start','lap','reset'):
   r=regs[n];box(n,caption=n.capitalize())
 elif s['id']=='shell':
  box('terminal',pal['terminal'],pal['terminal']);d.text((4,37),'VQEAF OS Shell',font=f10,fill='white');d.text((4,62),'Type help for commands',font=f9,fill='#77D976');box('prompt',pal['terminal']);d.text((5,263),'vqeaf:/$',font=f11,fill='#00FF70')
 elif s['id']=='browser':
  d.text((12,56),'Qeafbrowser',font=f13,fill=pal['text']);d.text((12,100),'WiFi is not connected',font=f10,fill=pal['text']);d.text((12,121),'Options > Home can open cache',font=f9,fill=pal['text'])
 elif s['id'] in ('wifi','bluetooth','files','gallery','installer'):
  cap={'wifi':'WIFI','bluetooth':'Bluetooth','files':'File manager','gallery':'Gallery','installer':'App installer'}[s['id']]
  d.text((12,54),cap,font=f13,fill=pal['text'])
  empty={'wifi':'No networks found','bluetooth':'No BLE devices found','files':'microSD is not mounted','gallery':'microSD is not mounted','installer':'microSD is not available'}[s['id']]
  d.text((12,106),empty,font=f10,fill=pal['text']);d.text((12,129),'Options > Rescan' if s['id'] in ('wifi','bluetooth') else 'Options > Open',font=f9,fill=pal['text'])
 else:
  names={'music':['Audio output','Track', 'Shuffle / Repeat'], 'settings':['Theme','Backlight','Audio volume','Clock format','Auto WiFi strongest','Auto keypad lock'],'themes':['VQEAF Night','AMOLED Red','Black','VQEAF Lime'],'applications':['Shell','Open apps','Notes','Notifications','Text viewer','Recovery'],'recovery':['Boot status','Start normal mode','Enable Safe Mode','Clear recovery flags','Restart device']}[s['id']]
  for i,t in enumerate(names):
   y=29+i*42
   if y+40>298:break
   d.rectangle((2,y,234,y+40),fill=pal['selected'] if i==0 else pal['bg'],outline=pal['border'] if i==0 else pal['bg'])
   d.rounded_rectangle((10,y+8,34,y+30),radius=4,fill=['#1999DD','#FFE100','#66BB77','#C44366'][i%4]);d.text((46,y+8),t,font=f11,fill=pal['text'])
  if s['id']=='music': box('mini_player',caption='Audio: unavailable / volume')
 # Visible perimeter and pixel coordinate anchors; labels don't imply tested on LCD.
 return im
imgs=[]
for i,s in enumerate(J['screens']):
 im=render(s);im.save(DEST/f"v22_{i+1:02d}_{s['id']}_pixel_blueprint.png");imgs.append((s,im))
for name in ('home','menu'):
 i=next(im for s,im in imgs if s['id']==name)
 i.resize((720,960),Image.Resampling.NEAREST).save(DEST/f'v22_{name}_blueprint_3x.png')
# A contact sheet to compare each component's footprint (not a screenshot)
W,H=4*240+5*10,4*352+5*10
out=Image.new('RGB',(W,H),'#242D27');d=ImageDraw.Draw(out)
for i,(s,im) in enumerate(imgs):
 x=10+(i%4)*250;y=10+(i//4)*362
 d.text((x,y),f"{i+1:02d} {s['title']}",font=f11,fill='white')
 out.paste(im,(x,y+20))
out.save(DEST/'v22_16_screen_pixel_blueprints.png')
print('Generated 16 layout diagrams, two 3x focus blueprints and a 4x4 contact sheet')
