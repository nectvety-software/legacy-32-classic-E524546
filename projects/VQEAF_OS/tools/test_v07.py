#!/usr/bin/env python3
"""Regression gates for Symbian S3 OS v0.7 Recovery/Library/Qeafbrowser integration."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
def read(rel): return (ROOT / rel).read_text(encoding='utf-8')
passed=[]
def check(name, cond):
    if not cond: raise AssertionError(name)
    passed.append(name); print('PASS:', name)

board=read('include/BoardConfig.h')
types=read('src/core/Types.h')
uih=read('src/core/SymbianUI.h'); ui=read('src/core/SymbianUI.cpp')
main=read('src/main.cpp'); apph=read('src/apps/Apps.h'); apps=read('src/apps/Apps.cpp')
sys_h=read('src/services/SystemService.h'); sys=read('src/services/SystemService.cpp')
browser_h=read('src/services/BrowserService.h'); browser=read('src/services/BrowserService.cpp')
image=read('src/services/ImageViewerService.cpp')
storage=read('src/services/StorageService.cpp')
pio=read('platformio.ini')

# Hardware/UI status policy.
check('portrait board remains 240x320', 'SCREEN_W = 240' in board and 'SCREEN_H = 320' in board)
chrome=ui[ui.index('void SymbianUI::chrome'):ui.index('void SymbianUI::softkeys')]
check('titlebar status draws WiFi', 'drawStatusWifi' in chrome)
check('titlebar status draws battery', 'drawStatusBattery' in chrome)
check('titlebar status omits BLE and SD indicators', 'drawStatusBle' not in chrome and 'drawStatusSd' not in chrome)
idle=ui[ui.index('void SymbianUI::idleHome'):ui.index('void SymbianUI::idleClock')]
check('idle status omits BLE and SD indicators', 'drawStatusBle' not in idle and 'drawStatusSd' not in idle)
lock=ui[ui.index('void SymbianUI::lockScreen'):ui.index('void SymbianUI::lockClock')]
check('lock status has WiFi and battery only', 'drawStatusWifi' in lock and 'drawStatusBattery' in lock and 'drawStatusBle' not in lock and 'drawStatusSd' not in lock)

# Recovery/Safe Mode.
for token in ['bootPending','crashStreak','forceSafe']:
    check('recovery state '+token, token in sys)
check('watchdog panic brownout resets recognized', all(x in sys for x in ['ESP_RST_PANIC','ESP_RST_INT_WDT','ESP_RST_TASK_WDT','ESP_RST_WDT','ESP_RST_BROWNOUT']))
check('automatic safe mode after repeated crashes', 'crashStreak >= 2' in sys)
check('stable boot clears crash-loop marker', '>= 12000UL' in sys and 'markHealthy()' in sys)
check('hardware A recovery chord', 'digitalRead(Board::KEY_A) == LOW' in main and 'setSafeMode(true)' in main)
check('safe mode disables risky apps', all(x in main for x in ['s == ScreenId::BLE','s == ScreenId::Music','s == ScreenId::Browser']))
check('safe mode skips audio runtime', 'if (!systemService.safeMode()) music.update();' in main and 'if (!systemService.safeMode()) {' in main)
check('safe mode disables WiFi autoconnect', 'WiFi auto-connect disabled' in main and '!systemService.safeMode() && settings.data().wifiAuto' in main)
check('recovery app screen exists', 'class RecoveryApp' in apph and re.search(r'\bRecovery,', types) is not None and 'Enable Safe Mode' in apps)
check('recovery can clear flags and restart', 'clearRecoveryState()' in apps and 'ESP.restart()' in apps)

# Gallery/Library/Text viewer.
check('Gallery and TextViewer screens registered', 'Gallery' in types and 'TextViewer' in types)
check('launcher exposes Gallery and Library', '"Gallery"' in apps and '"Library"' in apps)
check('file manager routes images to Gallery', 'return ScreenId::Gallery' in apps and 'pendingOpenPath' in apps)
check('file manager routes text to TextViewer', 'return ScreenId::TextViewer' in apps)
check('gallery scans BMP JPEG PNG', all(x in apps for x in ['".bmp"','".jpg"','".jpeg"','".png"']))
check('gallery image decoder supports BMP', 'drawBmp' in image and '24-bit BMP' in image)
check('gallery image decoder supports JPEG via Qeafbrowser decoder', 'TJpg_Decoder.h' in image and 'TJpgDec.drawJpg' in image)
check('gallery image decoder supports PNG via Qeafbrowser decoder', 'PNGdec.h' in image and 'PNG_DRAW_CALLBACK' in image and 'getLineAsRGB565' in image)
check('image buffers prefer PSRAM', 'MALLOC_CAP_SPIRAM' in image)
check('JPEG edge clipping preserves MCU source stride', 'bitmap + yy * w' in image)
check('text viewer uses bounded page buffers', 'PAGE_LINES = 15' in apph and 'PAGE_STACK = 16' in apph and 'char pageLines[PAGE_LINES][40]' in apph)
check('text viewer streams from file offsets', 'loadPage(AppContext &ctx, uint32_t fileOffset)' in apph and '.seek(fileOffset)' in apps)

# Qeafbrowser in-OS adapter.
check('Browser screen/app registered', re.search(r'\bBrowser,', types) is not None and 'class BrowserApp' in apph)
check('launcher exposes Qeafbrowser', 'Qeafbrowser keypad web' in apps and 'ScreenId::Browser' in apps)
check('browser uses fixed line/link pools', 'MAX_LINES = 240' in browser_h and 'MAX_LINKS = 64' in browser_h)
check('browser response buffer is bounded to 32KB', 'BODY_CAP = 32768' in browser)
check('browser response buffer prefers PSRAM', 'heap_caps_malloc(BODY_CAP, MALLOC_CAP_SPIRAM' in browser)
check('browser HTTP and HTTPS enabled', '#include <HTTPClient.h>' in browser and '#include <WiFiClientSecure.h>' in browser)
check('browser TLS compatibility follows source', 'secure.setInsecure()' in browser)
check('browser explicit redirect loop', 'for (int hop = 0; hop < 6; ++hop)' in browser and 'HTTPC_DISABLE_FOLLOW_REDIRECTS' in browser)
check('browser resolves redirects relative to current URL', 'resolveUrl(requestUrl, location.c_str()' in browser)
check('browser preserves href case', 'htmlAttr(raw, "href"' in browser)
check('browser renders image alt fallback', 'htmlAttr(raw, "alt"' in browser)
check('browser WML/HTML styles from Qeafbrowser v2.2', 'BR_STYLE_H1' in browser_h and 'headingStyle' in browser and 'BR_STYLE_FOLDER' in browser)
check('browser Opera Mini style user agent', 'Opera Mini/4.5' in browser)
check('browser default home matches Qeafbrowser source', 'https://qeafivels.com/' in browser and 'https://qeafivels.com/' in apps)
check('browser supports back history', 'HISTORY_MAX = 16' in browser_h and 'goBack()' in browser_h+browser)
check('browser supports keypad URL entry', 'Web address' in apps and 'TextKeyboard' in apph)

# S60 app-opening interstitial and task resume.
check('opening application renderer exists', 'openingApp' in uih and 'Dang mo ung dung' in ui)
check('resume interstitial text exists', 'Dang tiep tuc' in ui)
check('launcher/idle/files/task sources show opening screen', all(x in main[main.index('static bool shouldShowOpening'):main.index('static void enterScreen')] for x in ['ScreenId::Idle','ScreenId::Launcher','ScreenId::Files','ScreenId::TaskSwitcher']))
check('task switcher can resume app state', 'takeResumeRequest' in apph and 'resumeTarget' in main)

# Build/hardening.
check('JPEG and PNG libraries pinned', 'TJpg_Decoder @ 1.1.0' in pio and 'PNGdec @ 1.1.6' in pio)
check('AppContext uses explicit C++11 constructor', 'AppContext(SymbianUI &uiRef' in apph)
check('ctx linker collision remains fixed', 'static AppContext appCtx{' in main and re.search(r'\bAppContext\s+ctx\b', main) is None)
check('TFT macro collision fix remains', all(x in board for x in ['TFT_DC_PIN','TFT_CS_PIN','TFT_RST_PIN']))

for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'}:
        t=p.read_text(encoding='utf-8')
        check('balanced braces '+str(p.relative_to(ROOT)), t.count('{') == t.count('}'))

print(f'PASS: {len(passed)}/{len(passed)} v0.7 gates')
