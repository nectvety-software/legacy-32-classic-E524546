#!/usr/bin/env python3
"""Regression gates for Symbian S3 OS v1.0 S60 Green 240x320 skin."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(rel): return (ROOT/rel).read_text(encoding='utf-8')
checks=[]
def check(name, cond):
    if not cond: raise AssertionError(name)
    checks.append(name); print('PASS:',name)
board=read('include/BoardConfig.h')
th=read('src/core/Theme.h')
uih=read('src/core/SymbianUI.h')
uic=read('src/core/SymbianUI.cpp')
apps=read('src/apps/Apps.cpp')
sett=read('src/services/SettingsStore.cpp')
seth=read('src/services/SettingsStore.h')
main=read('src/main.cpp')
sh=read('src/services/ShellService.cpp')
rd=read('README.md')
ch=read('CHANGELOG.md')
check('portrait width 240','SCREEN_W = 240' in board)
check('portrait height 320','SCREEN_H = 320' in board)
check('portrait rotation 0','TFT_ROTATION = 0' in board)
check('S60 Green theme enum','S60Green = 2' in th)
check('S60 Green theme name','return "S60 Green"' in th)
check('default settings choose green','ThemeId theme = ThemeId::S60Green' in seth)
check('one-time theme migration','themeRev' in sett and 'ThemeId::S60Green' in sett)
check('27px titlebar','TITLEBAR_H = 27' in uih)
check('22px navbar','SOFTKEY_H = 22' in uih and 'SOFTKEY_TOP = 298' in uih)
check('launcher 3 columns','GRID_COLS = 3' in uih)
check('launcher 4 rows','GRID_ROWS = 4' in uih)
check('36px icon box','ICON_BOX = 36' in uih)
check('12 launcher apps','LAUNCH_COUNT = 12' in apps)
for title in ['WiFi','Bluetooth','Music','File mgr','Gallery','Internet','Shell','Recovery','Settings','Notes','Apps','Library']:
    check('launcher '+title, f'"{title}"' in apps)
check('launcher draws theme wallpaper','ctx.ui.menuBackground()' in apps)
check('green menu bands','greenRows[4]' in uic and 'menuRowColor' in uic)
check('green menu icons direct draw','drawS60MenuIcon' in uic)
check('no full menu bitmap','pushImage' not in uic and 'framebuffer' not in uic.lower())
check('status remains WiFi+battery only','status area is intentionally WiFi + battery only' in uic)
check('green theme selectable','themeName(s.theme)' in apps and 'ThemeId::S60Green' in apps)
check('reset appearance chooses green','s.theme = ThemeId::S60Green' in apps)
check('v1.0 version advertised',any(v in main for v in ['v1.1.0 Theme Engine','v1.0.1 S60 Green','v1.0 S60 Green']) and any(v in sh for v in ['Shell v1.1.0','Shell v1.0.1','Shell v1.0']))
# v0.9 file/network features must remain.
for cmd in ['mkdir','cp','mv','hexdump','netmon','nslookup','ping','wget','top']:
    check('retained shell '+cmd, f'cmd == "{cmd}"' in sh)
check('documentation v1.0',('# Symbian S3 OS v1.1' in rd or '# Symbian S3 OS v1.0' in rd) and 'v1.0.0 - S60 Green Theme' in ch and (ROOT/'docs/UI_V10.md').exists())
# source-brace sanity
for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'}:
        t=p.read_text(encoding='utf-8')
        check('balanced braces '+str(p.relative_to(ROOT)), t.count('{')==t.count('}'))
print(f'PASS: {len(checks)}/{len(checks)} v1.0 gates')
