#!/usr/bin/env python3
"""VQEAF OS 2.1.0 reproducible HOST checks. No real ESP32/Pio or live network."""
from pathlib import Path
import subprocess
import tempfile
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    process = subprocess.run([str(x) for x in args], cwd=ROOT, text=True,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if process.returncode:
        print(process.stdout[-7000:])
        raise SystemExit('FAILED: ' + ' '.join(map(str, args)))
    print(process.stdout.strip()[-1300:])
    return process


def check_previews():
    for name in ('v21_launcher_home_mock.png', 'v21_launcher_selected_mock.png'):
        blob = (ROOT/'preview'/name).read_bytes()[:24]
        assert blob[:8] == b'\x89PNG\r\n\x1a\n', name
        assert struct.unpack('>II', blob[16:24]) == (240, 320), name
    assert 'default_envs = vqeaf_os' in (ROOT/'platformio.ini').read_text()
    assert 'enum { W=240, H=320,' in (ROOT/'src/launcher/LauncherView.h').read_text()
    assert 'ScreenId::Launcher' in (ROOT/'src/main.cpp').read_text()
    assert 'KEY_SELECT' in (ROOT/'include/BoardConfig.h').read_text()
    assert (ROOT/'sd/System/Themes/vqeaf_night.vqeaf').is_file()
    assert (ROOT/'sd/System/Themes/vqeaf_day.vqeaf').is_file()
    print('PASS: source orientation, VQEAF default environment, boot route, sample themes and 240x320 mock PNGs')


def main():
    check_previews()
    with tempfile.TemporaryDirectory(prefix='vqeaf-g3-') as tmp:
        temp = Path(tmp)
        studio = temp/'theme_studio_host'
        run(['g++', '-std=c++11', '-Itools/theme_host', '-Isrc/services', '-Isrc/core',
             '-Isrc/launcher', 'tools/theme_host/test_vqeaf_studio.cpp',
             'src/services/ThemeFileService.cpp', '-o', studio])
        run([studio, 'themes/vqeaf_night.vqeaf'])
        input_test = temp/'input_t9_host'
        run(['g++', '-std=c++11', '-ffunction-sections', '-fdata-sections',
             '-DVQEAF_INPUT_FAKE_CLOCK', '-Itools/host_stubs', '-Iinclude', '-Isrc',
             'tools/vqeaf_host/test_input_t9.cpp', 'src/core/InputManager.cpp',
             'src/core/TextKeyboard.cpp', '-Wl,--gc-sections', '-o', input_test])
        run([input_test])
        launcher = temp/'launcher_host'
        run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
             '-Itools/vqeaf_host/preview_stubs', '-Itools/host_stubs',
             '-Iinclude', '-Isrc', 'tools/vqeaf_host/test_launcher.cpp',
             'src/launcher/LauncherView.cpp', '-o', launcher])
        run([launcher, temp/'launcher_render.ppm'])
    # Full v2.0 test includes the v1.9 stability matrix, mock builds of all
    # firmware translation units and old signed .qeapp tamper tests.
    run([sys.executable, 'tools/test_v20_storage_tls.py'])
    run([sys.executable, 'tools/test_v201_boarddiag.py'])
    print('PASS: VQEAF OS 2.1.0 G3 consolidated HOST validation')
    print('PENDING: PlatformIO target compile, flash, real ST7789 DMA, SD hotplug and physical key test')


if __name__ == '__main__':
    main()
