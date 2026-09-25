#!/usr/bin/env python3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(p): return (ROOT/p).read_text(encoding='utf-8')
checks=[]
def check(n,c):
    if not c: raise AssertionError(n)
    checks.append(n); print('PASS:',n)
uih=read('src/core/SymbianUI.h'); uic=read('src/core/SymbianUI.cpp'); th=read('src/core/Theme.h')
apps=read('src/apps/Apps.cpp'); rd=read('README.md'); ch=read('CHANGELOG.md')
check('portrait geometry','TITLEBAR_H = 27' in uih and 'SOFTKEY_TOP = 298' in uih and 'GRID_ROWS = 4' in uih)
check('compact status constants','STATUS_ICON_GAP = 6' in uih and 'STATUS_RIGHT_PAD = 6' in uih)
check('wifi and battery grouped','batteryX - STATUS_ICON_GAP - STATUS_ICON_W' in uic)
check('clock centered','(Board::SCREEN_W - tw) / 2' in uic)
check('amoled enum','AmoledRed = 3' in th)
check('amoled name','AMOLED Red' in th)
check('source screen palette','0x0000, // bg          #000000 screen' in th)
check('source accent palette','0xF9EB, // accent      #FF3D5B' in th)
check('source border palette','0x8967  // border      #8A2E3B' in th)
check('amoled s60 icons','themeId == ThemeId::S60Green || themeId == ThemeId::AmoledRed' in uic)
check('amoled menu background','Near-black row bands' in uic)
check('amoled wallpaper','AMOLED-friendly near-black wallpaper' in uic)
check('theme cycle includes red','ThemeId::AmoledRed ? ThemeId::Black' in apps)
check('device vqeaf red',(ROOT/'themes/amoled_red.vqeaf').exists())
check('device vqeaf green',(ROOT/'themes/s60_green.vqeaf').exists())
check('theme docs',(ROOT/'docs/THEMES_V11.md').exists())
check('version 1.1','v1.1.0 Theme Engine' in read('src/main.cpp') and '# Symbian S3 OS v1.1.0' in rd)
check('changelog v1.1','v1.1.0 - VQEAF AMOLED Red' in ch)
for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'}:
        txt=p.read_text(encoding='utf-8')
        check('balanced '+str(p.relative_to(ROOT)), txt.count('{')==txt.count('}'))
print(f'PASS: {len(checks)}/{len(checks)} v1.1 gates')
