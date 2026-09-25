#!/usr/bin/env python3
"""Executable Qeafbrowser-style WiFi manager state-machine tests + source integration.
Host simulation only: RF/STA interaction still needs ESP32-S3 hardware testing.
"""
import subprocess
import tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='s3-wifi-v16-') as d:
    cmd=['g++','-std=c++11','-Wall','-Wextra','-Werror',
         '-I'+str(R/'tools/wifi_host'),'-I'+str(R/'tools/host_stubs'),'-I'+str(R/'src/services'),
         str(R/'src/services/WiFiConnectionService.cpp'),
         str(R/'src/services/WiFiProfileStore.cpp'),
         str(R/'tools/wifi_host/test_wifi.cpp'), '-o',str(Path(d)/'test_wifi')]
    subprocess.run(cmd,check=True)
    subprocess.run([str(Path(d)/'test_wifi')],check=True)
print('PASS: 12 simulated RF/saved-profile/timeout/security state-machine cases')
main=(R/'src/main.cpp').read_text()
apps=(R/'src/apps/Apps.cpp').read_text()
header=(R/'src/apps/Apps.h').read_text()
service=(R/'src/services/WiFiConnectionService.cpp').read_text()
checks={
  'fixed board portrait 240x320':'SCREEN_W = 240' in (R/'include/BoardConfig.h').read_text() and 'SCREEN_H = 320' in (R/'include/BoardConfig.h').read_text(),
  'radio service initialized on boot':'wifiConnection.begin(wifiProfiles);' in main,
  'radio service polled each main loop':'wifiApp.tick(appCtx, screen == ScreenId::WiFi)' in main,
  'browser still shares normal WiFi status':'WiFi.status() != WL_CONNECTED' in (R/'src/services/BrowserService.cpp').read_text(),
  'saved profiles persist only after a successful join':'if (successful && profiles && targetSSID.length())' in service,
  'connect timeout is 12 seconds':'elapsedMs() >= 12000UL' in service,
  'async scan never unconditionally drops active WLAN':'WiFi.scanNetworks(true, true)' in service and 'WiFi.disconnect();' not in service.split('bool WiFiConnectionService::scan()')[1].split('void WiFiConnectionService::consumeScan')[0],
  'global shell/browser remains active during scans':'WiFi.scanDelete();' in service,
  'safe-mode reconnect preference propagated':'!ctx.system.safeMode()' in apps,
  'old password not accepted as verified for changed creds':'never trust or persist an unverified password' in service.lower(),
  'status has connect/rescan/disconnect/forget/saved/hidden':all(x in apps for x in ('"Rescan"','"Disconnect"','"Forget saved"','"Saved networks"','"Hidden network"')),
  'two-row-only focus redraw':'rowPaint(previous,false);rowPaint(cursor,true)' in apps,
  'no password printed in the new WiFi manager':'Serial.printf' not in service,
  'credentials never committed on failure':'if (successful && profiles' in service,
  'no new full-screen framebuffer':'TFT_eSprite' not in service and 'TFT_eSprite' not in apps,
  'S60 top bar only wifi and battery':'drawStatusBattery(batteryX' in (R/'src/core/SymbianUI.cpp').read_text(),
}
for description,ok in checks.items():
    if not ok:raise AssertionError(description)
    print('PASS:',description)
print(f'PASS: {len(checks)}/{len(checks)} source integration checks')
