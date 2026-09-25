#!/usr/bin/env python3
"""Generate ONLY host preview ASCII raster (not firmware font file)."""
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
D=Path(__file__).resolve().parent/'reference_stubs'
D.mkdir(exist_ok=True)
lines=['#pragma once','#include <stdint.h>','struct RefFontGlyph { uint8_t advance; uint16_t row[18]; };']
for idx,(file,size,baseline) in enumerate([('DejaVuSansMono.ttf',10,12),('DejaVuSansMono-Bold.ttf',12,15)]):
    fp='/usr/share/fonts/truetype/dejavu/'+file
    f=ImageFont.truetype(fp,size)
    lines.append(f'static const RefFontGlyph refFont{idx}[95]={{')
    for cp in range(32,127):
        im=Image.new('1',(16,18));ImageDraw.Draw(im).text((0,baseline),chr(cp),font=f,anchor='ls',fill=1)
        rows=[]
        for y in range(18):
            v=sum(1<<x for x in range(16) if im.getpixel((x,y)))
            rows.append(f'0x{v:04X}')
        adv=round(f.getlength(chr(cp)))
        lines.append('{%d,{%s}},'%(adv,','.join(rows)))
    lines.append('};')
(D/'ReferenceAsciiFont.h').write_text('\n'.join(lines)+'\n')
