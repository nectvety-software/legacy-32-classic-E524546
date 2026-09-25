#!/usr/bin/env python3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(p): return (ROOT/p).read_text(encoding='utf-8')
checks=[]
def check(n,c):
    if not c: raise AssertionError(n)
    checks.append(n); print('PASS:',n)
uih=read('src/core/SymbianUI.h'); uic=read('src/core/SymbianUI.cpp')
main=read('src/main.cpp'); rd=read('README.md'); ch=read('CHANGELOG.md')
check('portrait 240x320','STATUS_ZONE_W = 80' in uih and 'TITLEBAR_H = 27' in uih)
check('center zone + compact right group','STATUS_ZONE_W * 2' in uic and 'STATUS_ICON_GAP = 6' in uih)
check('clock exact center','(Board::SCREEN_W - tw) / 2' in uic)
check('clock vertical alignment','tft.setCursor((Board::SCREEN_W - tw) / 2, 9)' in uic)
check('wifi grouped right','wifiX = batteryX - STATUS_ICON_GAP - STATUS_ICON_W' in uic)
check('battery right aligned','batteryX = Board::SCREEN_W - STATUS_RIGHT_PAD - STATUS_ICON_W' in uic)
check('title pixel clipping','tft.textWidth(cut) > titleMaxW' in uic)
check('wifi bars cached','chromeWifiBars' in uih and 'wifiBars != chromeWifiBars' in uic)
check('wifi+battery only','no cellular modem/SIM' in uic)
check('idle same geometry','batteryX = Board::SCREEN_W - STATUS_RIGHT_PAD - STATUS_ICON_W' in uic)
check('version migrated','v1.1.0 Theme Engine' in main and '# Symbian S3 OS v1.1.0' in rd)
check('changelog 1.0.1','v1.0.1 - Centered clock / balanced S60 statusbar' in ch)
for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'}:
        txt=p.read_text(encoding='utf-8')
        check('balanced '+str(p.relative_to(ROOT)), txt.count('{')==txt.count('}'))
print(f'PASS: {len(checks)}/{len(checks)} v1.0.1 gates')
