#!/usr/bin/env python3
"""Executable host regression for v1.9; not an ESP32 target or RF test."""
from pathlib import Path
import subprocess, tempfile, sys
ROOT=Path(__file__).resolve().parents[1]


def run(*cmd):
    print('RUN:', ' '.join(str(x) for x in cmd),flush=True)
    subprocess.run([str(x) for x in cmd],cwd=ROOT,check=True)


def main():
  with tempfile.TemporaryDirectory(prefix='symbian-v19-') as d:
    base=Path(d)
    cxx=['g++','-std=c++11','-Wall','-Wextra']
    run(*cxx, '-ffunction-sections','-fdata-sections',
        '-Itools/host_stubs','-Iinclude','-Isrc',
        'tools/test_v19_url.cpp','src/services/BrowserService.cpp', 'src/services/TrustedTls.cpp',
        '-Wl,--gc-sections','-o',base/'url')
    run(base/'url')
    run(*cxx,'-Itools/v19_browser_host','-Itools/host_stubs','-Iinclude','-Isrc',
        'tools/v19_browser_host/test_browser_http.cpp','tools/v19_browser_host/extra.cpp',
        'src/services/BrowserService.cpp','src/services/TrustedTls.cpp','-o',base/'browser')
    run(base/'browser')
    run(*cxx,'-Itools/storage_host','-Itools/host_stubs','-Iinclude','-Isrc',
        'tools/storage_host/test_atomic.cpp','src/services/StorageService.cpp',
        '-o',base/'storage')
    (base/'card').mkdir()
    run(base/'storage',base/'card')
    run(*cxx,'-Werror','-Itools/v19_wifi_host','-Itools/host_stubs','-Isrc/services',
        'tools/v19_wifi_host/test_wifi_profile.cpp','src/services/WiFiProfileStore.cpp',
        '-o',base/'wifi')
    run(base/'wifi')
  for t in ['tools/test_grid_nav.py', # obsolete v1.2 layout literal replaced by v2.4.2 native theme test
             
            'tools/test_v16_wifi.py','tools/test_v17_auto_wifi.py',
            'tools/test_v18_core.py','tools/test_v14_build.py',
            'tools/test_v15_signature.py']:
    run(sys.executable,t)
  # Structural invariants that MUST NOT regress with this stability pass.
  board=(ROOT/'include/BoardConfig.h').read_text()
  ui=(ROOT/'src/core/SymbianUI.cpp').read_text()
  browser=(ROOT/'src/services/BrowserService.cpp').read_text()
  assert 'SCREEN_W = 240' in board and 'SCREEN_H = 320' in board
  assert 'tft.setTextColor(fg);' in ui
  assert 'timeText(bool hour12)' in ui and 'return String("--:--")' in ui
  assert '(uint32_t)(millis() - lastData) > 8000UL' in browser # current rollover-safe code
  print('PASS: v1.9 executable host stability gate; target PlatformIO not verified')

if __name__=='__main__':main()
