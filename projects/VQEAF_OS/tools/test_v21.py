#!/usr/bin/env python3
"""VQEAF v2.1 verification: real C++11 host source + simulated peripherals.

Does NOT substitute for a PlatformIO target firmware build or real ESP32 test.
"""
from pathlib import Path
import subprocess, sys, tempfile, re
R=Path(__file__).resolve().parents[1]

def run(*cmd):
    p=subprocess.run([str(x) for x in cmd],cwd=R,text=True,capture_output=True)
    if p.returncode:
        print('FAILED:',*cmd)
        print(p.stdout[-4500:]);print(p.stderr[-4500:])
        raise SystemExit(p.returncode)
    if p.stdout:
        print(p.stdout.strip()[-950:])
    if p.stderr:
        print('WARN:',p.stderr.strip()[-400:])

def check_source():
    board=(R/'include/BoardConfig.h').read_text()
    assert all(re.search(r'\b'+name+r'\s*=\s*'+str(val)+r'\s*;',board) for name,val in {
        'TFT_LEDK_PIN':39,'TFT_DC_PIN':47,'TFT_CS_PIN':14,'TFT_SCL_PIN':48,
        'TFT_SDA_PIN':12,'TFT_RST_PIN':3,'KEY_MENU':18,'KEY_UP':7,
        'KEY_A':15,'KEY_LEFT':45,'KEY_START':17,'KEY_RIGHT':6,
        'KEY_OPTION':8,'KEY_DOWN':46,'KEY_B':5,'KEY_SELECT':16,
        'SD_D3':10,'SD_CMD':11,'SD_CLK':13,'SD_D0':9}.items())
    assert 'SCREEN_W = 240' in board and 'SCREEN_H = 320' in board and 'TFT_ROTATION = 0' in board
    main=(R/'src/main.cpp').read_text()
    assert 'else enterScreen(ScreenId::Launcher, false);' in main
    assert 'physicalRecovery' in main and 'KEY_DOWN' in main
    assert 'vqeaf_os' in (R/'platformio.ini').read_text()
    apps=(R/'src/apps/Apps.cpp').read_text()
    assert 'cachedIconValid=ctx.installer.loadIcon' in apps
    assert 'return ScreenId::PackageApp' in apps
    loader=(R/'src/services/ThemeFileService.cpp').read_text()
    assert 'prefixCap = 16 * 1024' in loader and 'static bool hexColor' in loader
    assert 'Qeapp::' in (R/'src/services/AppInstallerService.cpp').read_text()
    print('PASS: unchanged hardware, boot-to-launcher, recovery, signed installed app routing')

def main():
    check_source()
    with tempfile.TemporaryDirectory(prefix='vqeaf-v21-') as td:
        tmp=Path(td)
        run('g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/theme_host',
            '-Isrc','-Isrc/services','-Isrc/core',
            'tools/vqeaf_host/test_theme_studio.cpp','src/services/ThemeFileService.cpp','-o',tmp/'theme')
        run(tmp/'theme','sd/Themes/vqeaf_night.vqeaf','sd/Themes/vqeaf_day.vqeaf')
        run('g++','-std=c++11','-Wall','-Wextra','-Werror','-ffunction-sections',
            '-fdata-sections','-DVQEAF_INPUT_FAKE_CLOCK','-Itools/host_stubs','-Iinclude','-Isrc',
            'tools/vqeaf_host/test_input_t9.cpp','src/core/InputManager.cpp',
            'src/core/TextKeyboard.cpp','-Wl,--gc-sections','-o',tmp/'input')
        run(tmp/'input')
        run('g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/vqeaf_host/preview_stubs',
            '-Itools/host_stubs','-Iinclude','-Isrc',
            'tools/vqeaf_host/test_launcher.cpp','src/launcher/LauncherView.cpp','-o',tmp/'launcher')
        run(tmp/'launcher',tmp/'preview.ppm')
    # Existing production module compilation, installer signature, WiFi and board diag.
    for test in ('test_v14_build.py','test_v15_signature.py','test_v16_wifi.py',
                 'test_v17_auto_wifi.py','test_v18_core.py','test_v201_boarddiag.py'):
        print('RUN',test)
        run(sys.executable,R/'tools'/test)
    print('VQEAF OS v2.1: host-only compatibility and structural gate PASS.')
    print('Target PlatformIO compile + physical LCD/SD/RF/audio remain PENDING.')

if __name__=='__main__':main()
