#!/usr/bin/env python3
"""Regression gates for Symbian S3 OS v0.6.1 S60 UI / anti-flicker pass."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')

checks=[]
def check(name, cond):
    if not cond:
        raise AssertionError(name)
    checks.append(name)
    print('PASS:', name)

ui_h=read('src/core/SymbianUI.h')
ui=read('src/core/SymbianUI.cpp')
main=read('src/main.cpp')
apps_h=read('src/apps/Apps.h')
apps=read('src/apps/Apps.cpp')
textkbd=read('src/core/TextKeyboard.cpp')
music=read('src/services/MusicService.cpp')
storage_h=read('src/services/StorageService.h')
storage=read('src/services/StorageService.cpp')
pio=read('platformio.ini')
notif=read('src/services/NotificationService.h')

# Portrait/S60 shell geometry.
check('portrait remains 240x320', 'SCREEN_W = 240' in read('include/BoardConfig.h') and 'SCREEN_H = 320' in read('include/BoardConfig.h'))
check('S60 titlebar/navbar geometry', all(x in ui_h for x in ['TITLEBAR_H = 27','CONTENT_TOP = 28','SOFTKEY_H = 22','SOFTKEY_TOP = 298']))
check('S60 icon cell retained/enlarged', 'ICON_BOX = 36' in ui_h and 'drawS60MenuIcon' in ui)
check('S60 titlebar keeps WiFi+battery status cluster', 'WiFi + battery only' in ui and 'drawStatusBattery' in ui)

# Anti-flicker architecture.
check('chrome redraw cache exists', 'chromeValid' in ui_h and 'sameShell' in ui and 'tm != chromeClock' in ui)
check('softkey redraw cache exists', 'softkeysValid' in ui_h and 'if (softkeysValid' in ui)
check('content clear is separate from full-screen clear', 'void SymbianUI::clearContent()' in ui and 'SOFTKEY_TOP - CONTENT_TOP' in ui)
check('keyboard clears content only on first frame', 'firstDraw' in read('src/core/TextKeyboard.h') and 'if (firstDraw) { ui.clearContent(); firstDraw = false; }' in textkbd)
check('idle focus redraw is partial', 'ui.idleShortcuts(idleShortcut)' in main and 'drawIdle(); }' not in main[main.find('if (screen == ScreenId::Idle)'):main.find('ScreenId next = screen;')])
check('idle/clock timer no longer full redraws every second', '>= 10000' in main and 'ui.idleClock(settings.data().hour12)' in main and 'drawClock(false)' in main)
check('launcher dpad redraws only old/new grid cells', 'ctx.ui.gridItem(old' in apps and 'ctx.ui.gridItem(index' in apps)
check('applications list redraws only old/new rows', ('ctx.ui.listItem(old, applicationIcon[old]' in apps) or ('rp(old,false);rp(index,true)' in apps and 'applicationIcon[item]' in apps))
check('WiFi list uses dirty old/new row repaint', 'paintRow(oldIndex, false);\n      paintRow(index, true);' in apps and 'String sub = String(nets[item].rssi)' in apps)
check('BLE list uses dirty old/new row repaint', 'ctx.ui.listItem(row, "BLE", devs[item].name' in apps)
check('File Manager uses dirty old/new row repaint', 'entries[item].isDir ? "Dir" : "File"' in apps and 'oldOffset != offset' in apps)
check('Music uses dirty old/new row repaint', 'ctx.ui.listItem(row, "Mus", tracks[item].name' in apps)
check('Settings uses dirty old/new row repaint', 'const char *labels[SETTINGS_COUNT]' in apps and 'paintRow(oldIndex, false)' in apps)
check('Quick Panel selection repaint is local', 'paintRow(oldIndex, false);\n    paintRow(index, true);' in apps and 'Lock keypad now' in apps)
check('Notifications selection repaint is local', 'const SystemNotification &n = ctx.notifications.at(item);' in apps)
check('Task switcher selection repaint is local', 'ScreenId recent = ctx.system.recentAt(item);' in apps)
check('transition no longer blanks whole LCD', 'never blanks the whole LCD' in ui and 'tft.fillScreen' not in ui[ui.find('void SymbianUI::transitionOut'):ui.find('void SymbianUI::progress')])

# RAM/flash pressure reductions.
check('Font4 removed from firmware build', 'LOAD_FONT4' not in pio and 'setTextFont(4)' not in (ui + main))
check('WiFi results are fixed char buffers and capped at 16', 'MAX_NETS = 16' in apps_h and 'char ssid[33]' in apps_h)
check('BLE results are fixed char buffers and capped at 16', 'MAX_DEVS = 16' in apps_h and 'char addr[18]' in apps_h)
check('file manager entry cache reduced', 'MAX_ENTRIES = 40' in apps_h)
check('music entry cache reduced', 'MAX_TRACKS = 32' in apps_h)
check('notification queue reduced', re.search(r'MAX_ITEMS\s*=\s*8', notif) is not None)
check('collection counts media without 64-entry temp array', 'countMedia(' in storage_h and 'FsEntry tmp[64]' not in apps)
check('audio work buffers reduced', 'static int16_t in[256]' in music and 'static int16_t out[512]' in music)
check('audio DMA pool reduced', 'dma_buf_count = 6' in music and 'dma_buf_len = 192' in music)
check('BLE host memory released after scan', 'NimBLEDevice::deinit(true)' in apps and 'Memory is released between scans' in apps)

# No re-introduction of previous linker/macro hazards.
check('app context still file-local and not named ctx', 'static AppContext appCtx{' in main and re.search(r'\bAppContext\s+ctx\b', main) is None)
board=read('include/BoardConfig.h')
check('TFT macro collision fix preserved', all(x in board for x in ['TFT_DC_PIN','TFT_CS_PIN','TFT_RST_PIN']) and 'constexpr int TFT_DC ' not in board)

# Structural sanity.
for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'}:
        t=p.read_text(encoding='utf-8')
        check('balanced braces '+str(p.relative_to(ROOT)), t.count('{') == t.count('}'))

print(f'PASS: {len(checks)}/{len(checks)} v0.6.1 gates')
