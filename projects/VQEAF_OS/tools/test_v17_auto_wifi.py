#!/usr/bin/env python3
"""WiFi auto-selection / reconnection simulator, not an RF hardware certification."""
from pathlib import Path
import subprocess, tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='wifi-auto-v17-') as tmp:
    cmd=['g++','-std=c++11','-Wall','-Wextra','-Werror',
         '-I'+str(r/'tools/wifi_host'),'-I'+str(r/'tools/host_stubs'),
         '-I'+str(r/'src/services'),
         str(r/'src/services/WiFiConnectionService.cpp'),
         str(r/'src/services/WiFiProfileStore.cpp'),
         str(r/'tools/wifi_host/test_auto_wifi.cpp'),'-o',tmp+'/run_wifi_auto']
    subprocess.run(cmd,check=True)
    subprocess.run([tmp+'/run_wifi_auto'],check=True)
print('PASS: WiFi auto-selection host test suite (10 scenarios)')
