#!/usr/bin/env python3
"""Extract 12 icon pairs from the user's generated contact sheet.

Use only to regenerate curated PNG sources if artwork is deliberately updated.
Normal firmware builds consume the checked-in 24x24/36x36 transparent PNGs
and do NOT require Pillow or the original contact sheet.
"""
from pathlib import Path
from PIL import Image
import argparse, math
from collections import deque
NAMES = ['wifi','bluetooth','music','files','gallery','internet','shell','recovery','settings','themes','apps','library']
# Tightly crop only the illustrated subject, never the contact sheet's white UI frame.
BOXES = {
 'wifi': ((79,109,214,229),(307,128,402,209)),
 'bluetooth': ((568,109,661,233),(781,133,852,214)),
 'music': ((1020,109,1144,238),(1223,133,1314,221)),
 'files': ((89,367,217,473),(310,375,404,461)),
 'gallery': ((535,359,670,485),(763,379,872,476)),
 'internet': ((1019,361,1143,489),(1226,382,1328,477)),
 'shell': ((82,623,218,739),(312,625,411,714)),
 'recovery': ((555,616,671,739),(766,626,873,725)),
 'settings': ((1030,621,1144,739),(1232,623,1324,718)),
 'themes': ((91,864,217,981),(306,868,412,966)),
 'apps': ((546,864,672,980),(762,878,874,974)),
 'library': ((1017,868,1144,991),(1223,882,1327,979)),
}
def is_background(rgb):
 r,g,b=rgb[:3]
 # All 24 backgrounds are the contact sheet's very pale lime panel.
 return (r>=175 and g>=225 and 80<=b<=215 and g>r+4 and g>b+18)
def extract(image,box,n):
 crop=image.crop(box).convert('RGB')
 w,h=crop.size
 # Flood-fill ONLY connected pale-lime background from the crop edges.
 # Preserves enclosed light-green globe continents and white artwork highlights.
 px=crop.load(); bg=[[False]*w for _ in range(h)]; queue=deque()
 for y in range(h):
  for x in (0,w-1):
   if is_background(px[x,y]):bg[y][x]=True;queue.append((x,y))
 for x in range(w):
  for y in (0,h-1):
   if is_background(px[x,y]) and not bg[y][x]:bg[y][x]=True;queue.append((x,y))
 while queue:
  x,y=queue.popleft()
  for xx,yy in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
   if 0<=xx<w and 0<=yy<h and not bg[yy][xx] and is_background(px[xx,yy]):
    bg[yy][xx]=True;queue.append((xx,yy))
 mask=[[not bg[y][x] for x in range(w)] for y in range(h)]
 # Guard against the source page's white square border touching the crop.
 # All 12 boxes are subject-tight; do not delete white accents/shadows.
 bbox=[x for y in range(h) for x in range(w) if mask[y][x]]
 if not bbox: raise ValueError(f'empty extraction {box}')
 xs=[x for y in range(h) for x in range(w) if mask[y][x]]
 ys=[y for y in range(h) for x in range(w) if mask[y][x]]
 xmin,xmax=min(xs),max(xs);ymin,ymax=min(ys),max(ys)
 sw,sh=xmax-xmin+1,ymax-ymin+1
 safe=4 if n==36 else 3
 content=n-2*safe
 scale=min(content/sw,content/sh)
 dw=max(1,round(sw*scale));dh=max(1,round(sh*scale))
 out=Image.new('RGBA',(n,n),(0,0,0,0))
 ox=(n-dw)//2;oy=(n-dh)//2
 # Supersampled opaque voting; ignore background RGB for anti-aliased pixels.
 for j in range(dh):
  for i in range(dw):
   xl=xmin+int(i*sw/dw);xr=xmin+max(int((i+1)*sw/dw),int(i*sw/dw)+1)
   yl=ymin+int(j*sh/dh);yr=ymin+max(int((j+1)*sh/dh),int(j*sh/dh)+1)
   vals=[px[x,y] for y in range(yl,min(yr,h)) for x in range(xl,min(xr,w)) if mask[y][x]]
   total=max(1,(min(xr,w)-xl)*(min(yr,h)-yl))
   if len(vals)<total*.42:continue
   # Middle opaque color: minimizes shimmering where source antialiases the edge.
   col=tuple(sorted(z[k] for z in vals)[len(vals)//2] for k in range(3))
   out.putpixel((ox+i,oy+j),(*col,255))
 return out

def main():
 ap=argparse.ArgumentParser();ap.add_argument('source',type=Path);ap.add_argument('--out',type=Path,default=Path('assets/pixel_icons')); args=ap.parse_args()
 im=Image.open(args.source).convert('RGB');args.out.mkdir(parents=True,exist_ok=True)
 for name in NAMES:
  for n,box in zip((36,24), BOXES[name]):
   png=extract(im,box,n); path=args.out/f'{name}_{n}.png';png.save(path,optimize=True)
   print(f'{path} {n}x{n} opaque={sum(1 for a in (png.getchannel("A").get_flattened_data() if hasattr(png.getchannel("A"),"get_flattened_data") else png.getchannel("A").getdata()) if a)}')
if __name__=='__main__':main()
