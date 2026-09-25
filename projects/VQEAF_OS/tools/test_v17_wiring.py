#!/usr/bin/env python3
"""v1.7 source integration assertions; simulate RF separately with test_v17_auto_wifi.py."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
main=(r/'src/main.cpp').read_text()
uih=(r/'src/core/SymbianUI.h').read_text()
uic=(r/'src/core/SymbianUI.cpp').read_text()
a=(r/'src/apps/Apps.cpp').read_text()
s=(r/'src/services/WiFiConnectionService.cpp').read_text()
h=(r/'src/services/WiFiConnectionService.h').read_text()
board=(r/'include/BoardConfig.h').read_text()
checks={
 'portrait LCD 240x320':'SCREEN_W = 240' in board and 'SCREEN_H = 320' in board,
 'saved profile count bounded':'MAX_PROFILES = 5' in (r/'src/services/WiFiProfileStore.h').read_text(),
 'nonblocking boot config': 'wifiConnection.configureAuto(!systemService.safeMode() && settings.data().wifiAuto)' in main,
 'legacy last SSID boot join removed':'WiFi.begin(saved.ssid.c_str()' not in main,
 'full scan before driver buffers freed':s.index('int16_t strength[')<s.index('WiFi.scanDelete();\n  state = Phase::Idle;',s.index('void WiFiConnectionService::consumeScan')),
 'signals sorted descending': 'candidateRssi[pos] > strength[p]' in s,
 'last SSID tie-break only':'candidateRssi[pos] == strength[p]' in s and 'lastSSID()' in s,
 'WPA/open profiles matched': 'profiles->at(p).open != open' in s,
 'async driver scan': 'WiFi.scanNetworks(true, true)' in s,
 'nonblocking main poll': 'wifiApp.tick(appCtx, screen == ScreenId::WiFi)' in main,
 'per network timeout':'elapsedMs() >= 12000UL' in s,
 'bounded retry':'autoBackoffMs < 120000UL' in s,
 '5s link-loss debounce':'disconnectedAt) >= 5000UL' in s,
 'no duplicate boot notifications':'autoJoinOwner = false;\n        autoState = AutoPhase::Online;' in s,
 'browse-only WiFi list resumes': 'wifiConnection.resumeAutoAfterBrowsing()' in main,
 'intentional disconnect stops retry':'suspendAuto(true); // an intentional Disconnect' in s,
 'Settings live auto control':'ctx.wifiConnection.configureAuto(s.wifiAuto && !ctx.system.safeMode());' in a,
 'QuickPanel radio disables auto':'ctx.wifiConnection.configureAuto(false);' in a,
 'safe mode suppresses boot scan':'!systemService.safeMode() && settings.data().wifiAuto' in main,
 'S60 idle home uses actual wifi status':'wifiHomeText()' in main and 'WiFi.RSSI()' in main,
 'standby status partial redraw':'tft.fillRect(18, 137, 205, 15, panel)' in uic and 'idleNetworkStatus' in uih,
 'idle redraw only on content change':'next != lastIdleWiFiText || live != lastIdleWiFiConnected' in main,
 'status bar still only wifi and battery':'drawStatusWifi(wifiX' in uic and 'drawStatusBattery(batteryX' in uic,
 'browser keeps shared WiFi station':'WiFi.status() != WL_CONNECTED' in (r/'src/services/BrowserService.cpp').read_text(),
 'no extra TFT framebuffer':'TFT_eSprite' not in s and 'TFT_eSprite' not in main,
 'shell radio manual override': 'radioService->externalOverride()' in (r/'src/services/ShellService.cpp').read_text() and 'shellService.begin(storage, systemService, &wifiConnection)' in main,
 'foreground app WiFi icon dirty refresh': 'ui.refreshWifiBadge(wifiConnected(), settings.data().hour12)' in main,
 'v1.7 docs':(r/'docs/WIFI_V17_AUTOCONNECT.md').exists(),
}
for name,condition in checks.items():
    if not condition: raise AssertionError(name)
    print('PASS:',name)
print('PASS:',len(checks),'source integration assertions')
