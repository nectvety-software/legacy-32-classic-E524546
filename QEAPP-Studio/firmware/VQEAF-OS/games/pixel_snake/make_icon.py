#!/usr/bin/env python3
"""Create an original 32x32 integer-pixel RGB icon, no external/font assets."""
from pathlib import Path
from PIL import Image, ImageDraw
im=Image.new('RGB',(32,32),'#172d28')
d=ImageDraw.Draw(im)
d.rectangle([0,0,31,31],fill='#152b25',outline='#89d855',width=1)
for y in range(3,29,5):
    for x in range(3,29,5):
        if (x+y)//5%2:d.rectangle([x,y,x+4,y+4], fill='#183a2f')
# Apple with golden stem.
d.rectangle((7,7,11,8),fill='#9acb3c');d.rectangle((6,9,12,15),fill='#ee6047')
d.rectangle((8,10,9,12),fill='#ffc183')
# Each segment drawn without anti-aliasing; green glow palette.
for x,y in [(5,23),(10,23),(15,23),(15,18),(20,18),(20,13)]:
    d.rectangle((x,y,x+5,y+5),fill='#084a30')
    d.rectangle((x+1,y+1,x+4,y+4),fill='#7bea56')
    d.line((x+1,y+1,x+4,y+1),fill='#e7ffab')
d.rectangle((24,14,24,14),fill='#172d28');d.rectangle((24,17,24,17),fill='#172d28')
out=Path(__file__).with_name('snake_icon_32.png')
im.save(out)
print(out)
