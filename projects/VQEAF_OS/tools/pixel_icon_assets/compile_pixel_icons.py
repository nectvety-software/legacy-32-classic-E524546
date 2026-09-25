#!/usr/bin/env python3
"""Build VQEAF OS pixel sprites WITHOUT changing their original RGB565 pixels.

v2.3.5: lossless bounds crop + cross-row RLE5, optional 4-bit palette for
small palettes. A reserved, unused index 31 (0xF8) encodes long transparent
runs. Firmware uses no heap, no PNG library, and no temporary icon buffer.

Use the same source PNGs and RGB565 palette quantizer as v2.3.4. The palette
mapping is intentionally unchanged: generated output is pixel-identical to
v2.3.4 (not necessarily lossless relative to original 24-bit PNG input).
"""
from collections import Counter
from pathlib import Path
from PIL import Image
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'tools/pixel_icon_assets/png'
DST = ROOT / 'src/core/VqeafIconData.h'
STATS = ROOT / 'tools/pixel_icon_assets/icon_build_stats.json'
NAMES = ('wifi','bluetooth','music','files','gallery','internet',
         'shell','recovery','settings','themes','apps','library')
CODEC_RLE5 = 0
CODEC_NIBBLE4 = 1


def rgb565(color):
    r,g,b=color[:3]
    return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3)


def unpack565(c):
    return ((c>>11 &31)*255//31, (c>>5&63)*255//63, (c&31)*255//31)


def quantize_palette(img):
    # Retain the EXACT previous quantizer and ordering; no re-paletting.
    px = list(img.get_flattened_data() if hasattr(img, 'get_flattened_data') else img.getdata())
    opaque = [(r,g,b) for r,g,b,a in px if a >= 128]
    distinct = Counter(map(rgb565, opaque))
    if len(distinct) <= 31:
        colors = [c for c,_ in sorted(distinct.items(), key=lambda kv:(-kv[1],kv[0]))]
        mapping = {c:i+1 for i,c in enumerate(colors)}
        return [0]+colors, [0 if a<128 else mapping[rgb565((r,g,b))] for r,g,b,a in px]
    strip = Image.new('RGB', (len(opaque), 1)); strip.putdata(opaque)
    q = strip.quantize(colors=30, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    keys = [rgb565(tuple(q.getpalette()[i*3:i*3+3])) for i in range(30)]
    colors = list(dict.fromkeys(keys))
    if len(colors) > 31:
        raise ValueError('Overfull RGB565 palette')
    def nearest(c):
        r,g,b=unpack565(c)
        return min(range(len(colors)), key=lambda i: sum((a-b)**2 for a,b in zip((r,g,b),unpack565(colors[i]))))+1
    mapping = {c:nearest(c) for c in distinct}
    return [0]+colors, [0 if a<128 else mapping[rgb565((r,g,b))] for r,g,b,a in px]


def legacy_rle(indices, width):
    """Original v2.3.4 horizontal-row RLE for baseline size verification."""
    out=[]
    for start in range(0,len(indices),width):
        row=indices[start:start+width]; pos=0
        while pos<len(row):
            color=row[pos];run=1
            while pos+run<len(row) and run<8 and row[pos+run]==color:
                run+=1
            out.append((color<<3)|(run-1));pos+=run
    return bytes(out)


def compile_image(name, n):
    """Stable reference API used by regression tests: (palette, old RLE, pixels)."""
    image_file = SRC / f'{name}_{n}.png'
    img = Image.open(image_file).convert('RGBA')
    if img.size != (n,n):
        raise ValueError(f'{name}_{n}: invalid size {img.size}')
    palette,indices=quantize_palette(img)
    safe=4 if n==36 else 3
    for y in range(n):
        for x in range(n):
            if x<safe or y<safe or x>=n-safe or y>=n-safe:
                if indices[y*n+x]:
                    raise ValueError(f'{name}_{n}: pixel outside safe margin at {x},{y}')
    if len(palette)>31:  # reserve role 31 as an escape token
        raise ValueError(f'{name}_{n}: palette too large for reserved escape role')
    return palette,legacy_rle(indices,n),indices


def crop(indices, n):
    coords=[(i%n,i//n) for i,value in enumerate(indices) if value]
    if not coords:
        raise ValueError('An all-transparent icon is not a valid system icon')
    x0=min(x for x,y in coords);x1=max(x for x,y in coords)+1
    y0=min(y for x,y in coords);y1=max(y for x,y in coords)+1
    values=[indices[y*n+x] for y in range(y0,y1) for x in range(x0,x1)]
    return (x0,y0,x1-x0,y1-y0),values


def encode_rle5(flat):
    """One-byte role5/run3 across rows; F8,<extra> for transparent 9..263."""
    out=bytearray(); pos=0
    while pos<len(flat):
        role=flat[pos];run=1
        while pos+run<len(flat) and flat[pos+run]==role:
            run+=1
        remaining=run
        while remaining:
            if role==0 and remaining>=16:
                length=min(remaining,263)
                out.extend((0xF8,length-8))
            else:
                length=min(remaining,8)
                out.append((role<<3)|(length-1))
            remaining-=length
        pos+=run
    return bytes(out)


def encode_nibble4(flat):
    if max(flat)>15:
        raise ValueError('A 4-bit icon palette cannot hold this role')
    return bytes((flat[i]<<4)|(flat[i+1] if i+1<len(flat) else 0)
                 for i in range(0,len(flat),2))


def decode_rle5(blob, length, palette_len):
    """Independent host decoder to reject overlong/undersized streams."""
    out=[];at=0
    while len(out)<length:
        if at>=len(blob):
            raise ValueError('Truncated RLE')
        token=blob[at];at+=1
        if token==0xF8:
            if at>=len(blob):
                raise ValueError('Truncated extended transparent run')
            role=0;length_run=8+blob[at];at+=1
        else:
            role=token>>3;length_run=(token&7)+1
        if role>=palette_len or len(out)+length_run>length:
            raise ValueError('RLE role/length out of range')
        out.extend([role]*length_run)
    if at!=len(blob):
        raise ValueError('Trailing RLE tokens')
    return out


def encode_image(name,n):
    pal,original,indices=compile_image(name,n)
    (x,y,w,h),flat=crop(indices,n)
    rle=encode_rle5(flat)
    nibble=encode_nibble4(flat) if len(pal)<=16 else None
    use_nibble = nibble is not None and len(nibble)<len(rle)
    codec=CODEC_NIBBLE4 if use_nibble else CODEC_RLE5
    body=nibble if use_nibble else rle
    # Generator self-test: every RGB565 palette index MUST be preserved.
    expanded=([z for b in body for z in (b>>4,b&15)][:len(flat)]
              if use_nibble else decode_rle5(body,len(flat),len(pal)))
    if expanded!=flat:
        raise AssertionError(f'Incorrect compression for {name}_{n}')
    if len(body)>65535 or max(flat)>=31:
        raise ValueError('Encoder output outside firmware constraints')
    return {
        'name':name,'size':n,'pal':pal,'body':body,'codec':codec,
        'bbox':(x,y,w,h),'opaque':sum(bool(z) for z in flat),
        'legacy_bytes':len(original)+2*len(pal),
        'source_sha256':hashlib.sha256((SRC/f'{name}_{n}.png').read_bytes()).hexdigest(),
    }


def format_array(values, conv):
    return '\n'.join('  '+','.join(conv(x) for x in values[i:i+14])+','
                     for i in range(0,len(values),14))


def create():
    all_assets=[]
    for n in (24,36):
        for name in NAMES:
            all_assets.append(encode_image(name,n))
    parts=[
        '// AUTOGENERATED by tools/pixel_icon_assets/compile_pixel_icons.py.',
        '// v2.3.5 LOSSLESS vs v2.3.4 RGB565 output; keep source PNGs immutable.',
        '// Full icon 24x24/36x36; store ONLY nontransparent bounding rectangles.',
        '// Optimized 5-bit local palette (legacy visual-test identification).',
        '// Codec 0: flat cross-row RLE5 + F8 ext transparent; codec 1: 4-bit.',
        '// Packed 16-byte Asset descriptors on ESP32-S3 (32-bit pointers).',
        '#pragma once','#include <stdint.h>','namespace VqeafIconData {',
        'enum { CODEC_RLE5 = 0, CODEC_NIBBLE4 = 1 };',
        'struct Asset {',
        '  const uint8_t *rle;',
        '  const uint16_t *rgb565;',
        '  uint16_t bytes;',
        '  uint8_t palette_len, x0, y0, w, h, codec;',
        '};',
        '#if UINTPTR_MAX == 0xFFFFFFFFu',
        'static_assert(sizeof(Asset)==16, "ESP32 descriptor must remain 16 bytes");',
        '#endif',
    ]
    stats={}
    for a in all_assets:
        key=f"{a['name']}_{a['size']}";pal=a['pal'];body=a['body']
        parts += [f'static const uint8_t {key}[] = {{',
                  format_array(body,lambda z:f'0x{z:02X}'), '};',
                  f'static const uint16_t {key}_rgb[] = {{',
                  format_array(pal,lambda z:f'0x{z:04X}'), '};']
        stats[key]={
            'old_bytes':a['legacy_bytes'], 'new_bytes':len(body)+2*len(pal),
            'stream_bytes':len(body), 'palette_bytes':2*len(pal),
            'codec':'nibble4' if a['codec'] else 'cropped_flat_rle5',
            'bbox_xywh':a['bbox'],'full_size':a['size'],
            'source_png_sha256':a['source_sha256'],
            'opaque_pixels':a['opaque'],
            'bytes':len(body)+2*len(pal), 'rle_bytes':len(body),
            'colors':len(pal)-1, 'size':a['size'],
        }
    for n in (24,36):
        parts.append(f'static const Asset ICONS_{n}[12] = {{')
        for a in all_assets:
            if a['size']!=n:continue
            key=f"{a['name']}_{n}"
            x,y,w,h=a['bbox']
            parts.append('  {' + f'{key},{key}_rgb, uint16_t(sizeof({key})), '
                         + f'{len(a["pal"])}, {x}, {y}, {w}, {h}, {a["codec"]}' + '},')
        parts.append('};')
    parts.append('} // namespace VqeafIconData')
    old_total=sum(s['old_bytes'] for s in stats.values())
    new_total=sum(s['new_bytes'] for s in stats.values())
    if new_total>=old_total:raise AssertionError('Icon pack was not reduced')
    report={
        'baseline_version':'2.3.4','version':'2.3.5',
        'pixel_output':'exact RGB565 parity with v2.3.4, preserving original 24 PNG sources',
        'asset_count':len(stats),'pixels_checked_expected':sum(12*n*n for n in (24,36)),
        'baseline_payload_bytes':old_total,'optimized_payload_bytes':new_total,
        'total_flash_bytes':new_total,  # backwards-compatible payload field
        'saved_payload_bytes':old_total-new_total,
        'saved_percent':round(100*(old_total-new_total)/old_total,2),
        'descriptor_size_esp32_before_and_after':16,
        'assets':stats,
    }
    DST.write_text('\n'.join(parts)+'\n',encoding='utf-8')
    STATS.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'PASS: {len(stats)} native sprites: {old_total} -> {new_total} bytes '
          f'(-{report["saved_percent"]}%, -{old_total-new_total} bytes payload); '
          'asset descriptors unchanged at 16 bytes on ESP32')
    return report


if __name__=='__main__':
    create()
