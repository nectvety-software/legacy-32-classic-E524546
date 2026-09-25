#!/usr/bin/env python3
"""Pixel pass regression: compile actual GUI and compare stable palette masks
against the supplied 240x320 screenshot crops. All preview text glyphs in the
PC TIFF stub differ from TFT_eSPI hardware fonts; don't pretend 1:1 LCD match.
"""
import subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(*c):
    p=subprocess.run([str(x) for x in c],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    if p.returncode:
        print('FAIL:',*c,'\n',p.stdout[-4200:]);raise SystemExit(p.returncode)
    if p.stdout.strip():print(p.stdout.strip()[-1200:])

def compare(preview):
    from PIL import Image,ImageChops,ImageDraw,ImageFont
    reference=ROOT/'tests/reference'
    out=ROOT/'preview'
    out.mkdir(exist_ok=True)
    pal={(41,109,24),(115,182,24),(222,242,156),(180,222,115),
         (172,222,74),(139,198,41),(148,214,49),(139,202,32),
         (74,125,41),(164,218,65),(255,255,255)}
    # v2.3.4 replaces the actual glyph artwork intentionally. Reference checks
    # only cover stable layout/chrome; tools/test_pixel_icons_v234.py tests every
    # icon pixel against the newly generated assets instead.
    new_art = '5-bit local palette' in (ROOT/'src/core/VqeafIconData.h').read_text()
    def in_new_glyph(name,x,y):
        if name=='home':
            return any(8+76*i+(72-36)//2<=x<8+76*i+(72-36)//2+36
                       and 197<=y<233 for i in range(3))
        return any(21+78*c<=x<21+78*c+36 and 31+66*r<=y<31+66*r+36
                   for c in range(3) for r in range(4))
    items=[]
    for name,limit in [('home',0.94),('menu',0.95)]:
        a=Image.open(reference/f'user_{name}_240x320.png').convert('RGB')
        b=Image.open(preview/f'{name}.ppm').convert('RGB')
        assert a.size==b.size==(240,320)
        stable=matching=0
        for y in range(320):
            for x in range(240):
                ra=a.getpixel((x,y))
                if new_art and in_new_glyph(name,x,y): continue
                if ra in pal:
                    stable+=1
                    if ra==b.getpixel((x,y)): matching+=1
        ratio=matching/stable
        suffix=' (intentionally updated icon rectangles excluded)' if new_art else ''
        print(f'PASS CHECK {name}: {matching}/{stable} screenshot palette-grounded pixels align ({ratio:.2%}); min {limit:.0%}{suffix}')
        assert ratio>limit, 'Regression of reference-image palette/layout masks'
        b.save(out/f'v23_{name}_host_raster.png')
        b.resize((720,960),Image.Resampling.NEAREST).save(out/f'v23_{name}_host_raster_3x.png')
        # Correctly labeled guide: reference and C++ host render, not board photographs.
        items.append((name,a,b,ratio))
    for name in ('home_selected','menu_selected'):
        src=Image.open(preview/f'{name}.ppm').convert('RGB')
        src.save(out/f'v23_{name}_host_raster.png')
        src.resize((720,960),Image.Resampling.NEAREST).save(out/f'v23_{name}_host_raster_3x.png')
    sheet=Image.new('RGB',(2*720+40,2*1000+45),'#26302C')
    d=ImageDraw.Draw(sheet)
    font='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    f=ImageFont.truetype(font,16) if Path(font).exists() else ImageFont.load_default()
    for i,(name,reference,host,ratio) in enumerate(items):
        y=20+i*1010
        d.text((20,y-17),f'{name.upper()} / User reference',font=f,fill='white')
        d.text((760,y-17),f'{name.upper()} / C++ host RGB565 raster ({ratio:.1%} palette area)',font=f,fill='white')
        sheet.paste(reference.resize((720,960),Image.Resampling.NEAREST),(20,y))
        sheet.paste(host.resize((720,960),Image.Resampling.NEAREST),(760,y))
    sheet.save(out/'v23_side_by_side_reference_vs_host.png')
    vi=Image.open(preview/'font_vi.ppm').convert('RGB');vi.save(out/'v23_vietnamese_font_host.png')
    vi.resize((720,960),Image.Resampling.NEAREST).save(out/'v23_vietnamese_font_host_3x.png')
    # Crop the exact 36px icon rectangles emitted by the firmware GUI.
    menu=Image.open(preview/'menu.ppm').convert('RGB')
    names=['WiFi','Bluetooth','Music','File mgr','Gallery','Internet','Shell','Recovery','Settings','Themes','Apps','Library']
    icons=Image.new('RGB',(4*118+20,3*110+20),'#26302C');dd=ImageDraw.Draw(icons)
    for i,name in enumerate(names):
        sx=21+78*(i%3); sy=31+66*(i//3)
        tile=menu.crop((sx,sy,sx+36,sy+36)).resize((72,72),Image.Resampling.NEAREST)
        x=10+(i%4)*118;y=10+(i//4)*110
        icons.paste(tile,(x+21,y))
        dd.text((x+6,y+77),name,font=f,fill='white')
    icons.save(out/'v23_unified_icon_catalog_host.png')
    (ROOT/'docs/PASS_V23_VISUAL.txt').write_text('\n'.join(f'{name}: {ratio:.3%} large palette pixels, host simulated fonts' for name,_,_,ratio in items)+'\n')

def main():
    # Real board source link; no fake claim for ESP32 compile or live peripherals.
    with tempfile.TemporaryDirectory(prefix='vqeaf-v23-') as t:
        d=Path(t);binary=d/'test_gui'
        run('g++','-std=c++11','-fpermissive','-Wall','-Wextra','-Werror',
            '-Itools/vqeaf_host/reference_stubs','-Itools/host_stubs','-Iinclude','-Isrc','-Isrc/core',
            'tools/vqeaf_host/test_v23_render.cpp','src/core/SymbianUI.cpp','src/core/VqeafIconRenderer.cpp',
            'tools/host_stubs/host_globals.cpp','-o',binary)
        run(binary,str(d)+'/')
        compare(d)
    # Structural baseline includes signed QEAPP, .vqeaf Studio, GPIO and Wi-Fi suites.
    run(sys.executable,'tools/test_ui_v22.py')
    src=(ROOT/'src/main.cpp').read_text()
    assert 'ui.idleShortcutDelta(old,idleShortcut)' in src
    assert 'else enterScreen(ScreenId::Idle, false);' in src
    assert 'VQEAF OS v2.' in src
    assert all((ROOT/'src/core'/file).exists() for file in
               ['UiTypography.h','UiIconCatalog.h','UiVietnameseFont.h','UiLayoutGeometry.h'])
    assert not list(ROOT.rglob('*.ttf')) and not list(ROOT.rglob('*.otf'))
    print('PASS VQEAF v2.3 source/host. PlatformIO firmware & board still PENDING.')
if __name__=='__main__':main()
