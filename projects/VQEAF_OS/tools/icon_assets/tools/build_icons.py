#!/usr/bin/env python3
"""VQEAF original icon art & firmware data: single-source 36-unit primitives.
Do not substitute third-party trademarks or copyrighted icon assets.
Outputs 12x SVG, 12x 24 PNG, 12x 36 PNG, flash RLE arrays & gallery.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from math import sin,cos,pi
import json
ROOT=Path(__file__).resolve().parents[1]
# Pixel token zero is 100% transparent; 1..13 semantic colors.
TOK={
 'clear':0, 'ink':1, 'white':2, 'cyan':3, 'blue':4, 'pink':5,
 'yellow':6,'orange':7,'green':8,'silver':9,'shadow':10,
 'mint':11,'red':12,'sky':13,
}
HEX={
 'clear':'#00000000','ink':'#23313A','white':'#FFFFFF','cyan':'#02CBED',
 'blue':'#235FD9','pink':'#FE2996','yellow':'#F5EB42','orange':'#F7A620',
 'green':'#16CD60','silver':'#D3E0E7','shadow':'#566675',
 'mint':'#93ED8F','red':'#F5544C','sky':'#69BDEB'
}
NAMES=[('wifi','WiFi','Wi'),('bluetooth','Bluetooth','BLE'),('music','Music','Mus'),
 ('files','File manager','Dir'),('gallery','Gallery','Pic'),('internet','Internet','Web'),
 ('shell','Shell','Term'),('recovery','Recovery','Rec'),('settings','Settings','Set'),
 ('themes','Themes','Th'),('apps','Apps','App'),('library','Library','Col')]

class Art:
 def __init__(self,name):self.name=name;self.shapes=[]
 def rect(self,x,y,w,h,fill,stroke=None,sw=1):self.shapes.append(('rect',x,y,w,h,fill,stroke,sw))
 def ellipse(self,x,y,w,h,fill,stroke=None,sw=1):self.shapes.append(('ellipse',x,y,w,h,fill,stroke,sw))
 def line(self,coords,color,sw=2):self.shapes.append(('line',coords,color,sw))
 def poly(self,coords,fill,stroke=None,sw=1):self.shapes.append(('poly',coords,fill,stroke,sw))
 def svg(self):
  def color(x):return HEX[x]
  z=['<svg xmlns="http://www.w3.org/2000/svg" width="36" height="36" viewBox="0 0 36 36" shape-rendering="crispEdges">', f'<title>VQEAF OS – {self.name}</title>']
  for a in self.shapes:
   k=a[0]
   if k in ('rect','ellipse'):
    _,x,y,w,h,fill,stroke,sw=a
    sty=f'fill="{color(fill)}"'+(f' stroke="{color(stroke)}" stroke-width="{sw}"' if stroke else '')
    if k=='rect': z.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" {sty}/>')
    else:z.append(f'<ellipse cx="{x+w/2}" cy="{y+h/2}" rx="{w/2}" ry="{h/2}" {sty}/>')
   elif k=='line':
    _,pts,ink,sw=a
    pp=' '.join(f'{pts[i]},{pts[i+1]}' for i in range(0,len(pts),2))
    z.append(f'<polyline points="{pp}" fill="none" stroke="{color(ink)}" stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round"/>')
   else:
    _,pts,fill,stroke,sw=a
    pp=' '.join(f'{pts[i]},{pts[i+1]}' for i in range(0,len(pts),2))
    sty=f'fill="{color(fill)}"'+(f' stroke="{color(stroke)}" stroke-width="{sw}"' if stroke else '')
    z.append(f'<polygon points="{pp}" {sty} stroke-linejoin="round"/>')
  return '\n'.join(z+['</svg>',''])
 def raster(self,n):
  s=n/36
  q=lambda x:round(x*s)
  im=Image.new('L',(n,n),TOK['clear']);d=ImageDraw.Draw(im)
  for a in self.shapes:
   k=a[0]
   if k in ('rect','ellipse'):
    _,x,y,w,h,fill,stroke,sw=a
    box=(q(x),q(y),q(x+w)-1,q(y+h)-1)
    args=dict(fill=TOK[fill]);
    if stroke:args.update(outline=TOK[stroke],width=max(1,q(sw)))
    (d.rectangle if k=='rect' else d.ellipse)(box,**args)
   elif k=='line':
    _,pts,fill,sw=a
    points=[(q(pts[i]),q(pts[i+1])) for i in range(0,len(pts),2)]
    weight=max(1,q(sw));d.line(points,fill=TOK[fill],width=weight,joint='curve')
    # square pixel ends for same anti-alias-free appearance across devices
   else:
    _,pts,fill,stroke,sw=a
    points=[(q(pts[i]),q(pts[i+1])) for i in range(0,len(pts),2)]
    d.polygon(points,fill=TOK[fill]);
    if stroke:d.line(points+[points[0]],fill=TOK[stroke],width=max(1,q(sw)),joint='curve')
  # Fully-transparent four-pixel (36), three-pixel (24) safety margin.
  border=4 if n==36 else 3
  for y in range(n):
   for x in range(n):
    if im.getpixel((x,y)) and (x<border or y<border or x>=n-border or y>=n-border):
     raise AssertionError(f'{self.name} {n} overdraws safe area at {(x,y)}')
  return im


def design(name):
 a=Art(name)
 if name=='wifi':
  a.line([6,14,10,10,14,8,18,7,22,8,26,10,30,14],'cyan',2)
  a.line([10,19,13,16,16,15,20,15,23,16,26,19],'cyan',2)
  a.line([14,23,16,21,20,21,22,23],'sky',2)
  a.ellipse(16,25,4,4,'orange')
 elif name=='bluetooth':
  a.rect(9,5,18,27,'blue','white',2)
  a.line([17,9,23,14,12,22],'white',2)
  a.line([17,27,23,21,12,14],'white',2)
  a.line([17,9,17,27],'white',1)
 elif name=='music':
  a.line([18,9,29,6,29,24],'pink',3)
  a.line([18,9,18,26],'pink',3)
  a.line([18,12,29,9],'pink',2)
  a.ellipse(9,23,9,7,'pink');a.ellipse(21,20,9,7,'pink')
  a.ellipse(24,25,7,7,'mint','white',1)
  a.poly([27,26,27,30,30,28],'green')
 elif name=='files':
  a.rect(5,11,27,19,'ink')
  a.rect(6,12,24,16,'orange')
  a.rect(8,8,12,5,'yellow')
  a.rect(7,13,22,2,'yellow')
  a.line([7,28,29,28],'shadow',1)
 elif name=='gallery':
  a.rect(8,5,23,25,'shadow','ink',1)
  a.rect(5,9,23,22,'silver','white',1)
  a.rect(8,12,17,15,'sky')
  a.ellipse(18,14,4,4,'yellow')
  a.poly([8,27,15,17,21,27],'green')
  a.poly([14,27,22,20,25,23,25,27],'mint')
 elif name=='internet':
  a.ellipse(6,6,24,24,'blue','ink',1)
  a.ellipse(9,6,18,24,'clear','white',1)
  a.line([6,18,29,18],'white',1)
  a.line([9,11,27,11],'sky',1)
  a.line([9,25,27,25],'sky',1)
  a.poly([7,14,13,10,18,13,17,18,11,19],'green')
  a.poly([21,19,28,18,27,25,23,27,19,24],'mint')
 elif name=='shell':
  a.rect(5,6,27,26,'shadow')
  a.rect(5,6,25,24,'ink','white',1)
  a.rect(6,7,23,5,'silver')
  a.rect(9,9,2,2,'red');a.rect(13,9,2,2,'yellow');a.rect(17,9,2,2,'green')
  a.line([10,17,15,21,10,25],'white',2)
  a.line([18,25,26,25],'mint',2)
 elif name=='recovery':
  # bounded two clockwise half loops and their arrowheads
  a.ellipse(7,7,22,22,'green','ink',1)
  a.ellipse(12,12,12,12,'clear','white',2)
  a.rect(14,6,9,4,'green')
  a.poly([22,5,29,10,22,15],'mint')
  a.poly([13,22,5,26,13,29],'white')
 elif name=='settings':
  pts=[]
  for k in range(24):
   angle=(2*pi*k/24)-pi/2
   radius=15 if k%3 in (0,1) else 11
   pts.extend([round(18+radius*cos(angle)),round(18+radius*sin(angle))])
  # widest teeth at 33 when rounding -> ensure safety margin => 14 radius
  pts=[round(18+(v-18)*12/15) for v in pts]
  a.poly(pts,'silver','shadow',1)
  a.ellipse(12,12,12,12,'shadow')
  a.ellipse(15,15,6,6,'cyan')
 elif name=='themes':
  a.ellipse(5,5,26,26,'silver','ink',1)
  a.ellipse(9,9,5,5,'red')
  a.ellipse(18,7,5,5,'blue')
  a.ellipse(24,15,5,5,'green')
  a.ellipse(10,22,5,5,'yellow')
  a.ellipse(20,22,6,6,'clear')
 elif name=='apps':
  a.rect(5,10,25,21,'ink')
  a.rect(6,11,24,18,'orange')
  a.rect(7,7,11,6,'yellow')
  a.rect(19,15,5,5,'blue');a.rect(25,15,5,5,'green');a.rect(22,22,6,6,'pink')
 elif name=='library':
  a.rect(12,6,20,22,'shadow','ink',1)
  a.rect(7,10,21,21,'silver','white',1)
  a.rect(10,13,15,15,'sky')
  a.ellipse(18,16,4,4,'yellow')
  a.poly([10,28,16,21,21,28],'green')
  a.poly([17,28,23,23,25,25,25,28],'mint')
 else:raise ValueError(name)
 return a

def packed_rows(im):
 n=im.width;pix=list(im.getdata());runs=[]
 for y in range(n):
  x=0
  while x<n:
   k=pix[y*n+x];length=1
   while x+length<n and pix[y*n+x+length]==k and length<16:length+=1
   assert 0<=k<=15
   runs.append(((k&0x0f)<<4)|(length-1))
   x+=length
 return bytes(runs)

def rgba(im):
 out=Image.new('RGBA',im.size,(0,0,0,0))
 target=[]
 for c in im.getdata():
  h=HEX[next(k for k,v in TOK.items() if v==c)]
  if c==0:target.append((0,0,0,0))
  else:target.append(tuple(int(h[j:j+2],16) for j in (1,3,5))+(255,))
 out.putdata(target);return out

def emit_data(assets):
 h=['// GENERATED BY tools/build_icons.py. Keep this file in flash, never allocate to PSRAM.',
    '// Nibble high = palette index 0..13, low = contiguous pixels - 1 (1..16).',
    '// Every row consists of runs totaling exactly icon.width. No row padding.',
    '#pragma once','#include <stdint.h>','namespace VqeafIconData {',
    'struct Asset { uint8_t width, height; uint16_t bytes; const uint8_t *rle; };']
 for name,n,data in assets:
  h.append('static const uint8_t %s_%d[] = {'%(name,n))
  chunks=[data[i:i+24] for i in range(0,len(data),24)]
  h+=['  '+','.join('0x%02X'%v for v in part)+',' for part in chunks]
  h.append('};')
 for n in (24,36):
  h.append('static const Asset ICONS_%d[12] = {'%n)
  for name,nn,data in assets:
   if nn==n:h.append('  {%d,%d,%d,%s_%d},'%(n,n,len(data),name,n))
  h.append('};')
 h.append('} // VqeafIconData')
 (ROOT/'firmware'/'VqeafIconData.h').write_text('\n'.join(h)+'\n')

def main():
 for n in (24,36):
  for ext in (f'assets/svg/{n}',f'assets/raw565/{n}'):(ROOT/ext).mkdir(parents=True,exist_ok=True)
 (ROOT/'assets/svg/geometry').mkdir(parents=True,exist_ok=True)
 assets=[];info=[]
 for name,title,legacy in NAMES:
  a=design(name)
  (ROOT/'assets/svg/geometry'/f'{name}.svg').write_text(a.svg())
  for n in (24,36):
   im=a.raster(n)
   rgba(im).save(ROOT/f'assets/png/{n}/{name}_{n}.png')
   # Exact 1px-run SVG uses the SAME color indices as firmware, including holes.
   svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{n}" height="{n}" viewBox="0 0 {n} {n}" shape-rendering="crispEdges">',
        f'<title>VQEAF OS – {name} – {n}px runtime matching</title>']
   pix=list(im.getdata());inv={val:key for key,val in TOK.items()}
   for yy in range(n):
    xx=0
    while xx<n:
     role=pix[yy*n+xx];r=1
     while xx+r<n and pix[yy*n+xx+r]==role:r+=1
     if role:
      svg.append(f'<rect x="{xx}" y="{yy}" width="{r}" height="1" fill="{HEX[inv[role]]}"/>')
     xx+=r
   svg.append('</svg>')
   (ROOT/f'assets/svg/{n}/{name}_{n}.svg').write_text('\n'.join(svg)+'\n')
   # Optional raw16 SD preview: LITTLE-ENDIAN RGB565, 0xF81F = transparent.
   def to565(h):
    rgb=[int(h[i:i+2],16) for i in (1,3,5)]
    return ((rgb[0]>>3)<<11)|((rgb[1]>>2)<<5)|(rgb[2]>>3)
   raw=bytearray()
   for v in pix:
    val=0xF81F if v==0 else to565(HEX[inv[v]])
    raw.extend((val&255,val>>8))
   (ROOT/f'assets/raw565/{n}/{name}_{n}.rgb565').write_bytes(raw)
   data=packed_rows(im)
   assets.append((name,n,data))
   info.append(dict(name=name,legacy=legacy,size=n,rleBytes=len(data),opaque=sum(1 for p in im.getdata() if p)))
 emit_data(assets)
 fontpath='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
 f=ImageFont.truetype(fontpath,14) if Path(fontpath).exists() else ImageFont.load_default()
 fb=ImageFont.truetype(fontpath,17) if Path(fontpath).exists() else ImageFont.load_default()
 bg='#23313A';cell='#8BC82C';focus='#DFF29C';nav='#235C1E'
 gallery=Image.new('RGB',(800,1070),bg);d=ImageDraw.Draw(gallery)
 d.text((34,22),'VQEAF OS  •  ICON SYSTEM v1.0',fill='white',font=fb)
 d.text((34,48),'12 glyphs  /  36x36 Menu  /  24x24 Lists',fill='#CADAD8',font=f)
 for i,(name,title,legacy) in enumerate(NAMES):
  col=i%3;row=i//3
  x=30+col*254;y=85+row*232
  d.rounded_rectangle((x,y,x+236,y+216),radius=8,fill=cell,outline='#315D25',width=2)
  d.rectangle((x+10,y+9,x+115,y+122),fill=focus if i==0 else '#C6EB77',outline='#FFFFFF' if i==0 else '#77A843',width=2)
  src=Image.open(ROOT/f'assets/png/36/{name}_36.png').convert('RGBA');large=src.resize((108,108),Image.Resampling.NEAREST)
  gallery.paste(large,(x+8,y+10),large)
  d.rectangle((x+136,y+26,x+224,y+114),fill='#DFF29C',outline='#FFFFFF',width=2)
  src2=Image.open(ROOT/f'assets/png/24/{name}_24.png').convert('RGBA');lg=src2.resize((72,72),Image.Resampling.NEAREST);gallery.paste(lg,(x+144,y+32),lg)
  d.text((x+14,y+139),title,fill='#101E16',font=fb)
  d.text((x+14,y+171),'36 px           24 px',fill='#1C3B1F',font=f)
 d.rectangle((30,1022,765,1024),fill='#5D7E59');d.text((32,1031),'Original artwork · RGB565 firmware RLE · No branded assets',fill='#C8DFD6',font=f)
 gallery.save(ROOT/'VQEAF_OS_12_Icon_Catalog.png')
 for n in (24,36):
  atlas=Image.new('RGBA',(4*n,3*n),(0,0,0,0))
  mapping=[]
  for i,(name,title,legacy) in enumerate(NAMES):
   x=(i%4)*n;y=(i//4)*n
   atlas.paste(Image.open(ROOT/f'assets/png/{n}/{name}_{n}.png'),(x,y))
   mapping.append({'id':i,'name':name,'legacy':legacy,'rect':[x,y,n,n]})
  atlas.save(ROOT/f'assets/atlas_{n}.png')
  (ROOT/f'assets/atlas_{n}.json').write_text(json.dumps(mapping,ensure_ascii=False,indent=2)+'\n')
 (ROOT/'docs'/'ICON_STATS.json').write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n')
 print('PASS generated',len(assets),'PNG; RLE total bytes',sum(len(v) for _,_,v in assets))

if __name__=='__main__':main()
