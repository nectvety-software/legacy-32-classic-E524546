#!/usr/bin/env python3
"""VQEAF OS 2.2 UI/Layout host regression. Offline, no physical device claims."""
from pathlib import Path
import re, json, subprocess, sys, tempfile
ROOT=Path(__file__).resolve().parents[1]
def run(*args):
    p=subprocess.run([str(x) for x in args],cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if p.returncode:
        print('FAIL COMMAND:', *args, '\n', p.stdout[-6000:]);sys.exit(p.returncode)
    print(p.stdout[-1000:].strip())

def static():
    p=json.loads((ROOT/'docs/pixel_atlas.json').read_text('utf-8'))
    assert (p['screen_width'],p['screen_height'],len(p['screens']))==(240,320,16)
    assert sum(len(s['regions']) for s in p['screens'])==162
    assert len({s['id'] for s in p['screens']})==16
    for s in p['screens']:
        for v in s['regions']:
            assert all(type(v[q])==int for q in ('x','y','w','h'))
            assert 0<=v['x']<240 and 0<=v['y']<320 and v['w']>0 and v['h']>0
            assert v['x']+v['w']<=240 and v['y']+v['h']<=298
    menu=next(z for z in p['screens'] if z['id']=='menu')
    assert len([z for z in menu['regions'] if z['id'].startswith('cell_')])==12
    assert (ROOT/'src/apps/LauncherGrid.cpp').exists()
    m=(ROOT/'src/main.cpp').read_text()
    assert 'else enterScreen(ScreenId::Idle, false);' in m, 'Boot MUST show screenshot-style Home'
    assert 'else enterScreen(ScreenId::Idle, false);' in m[m.index('if (screen != ScreenId::Launcher) enterScreen'):], 'MENU in grid to Home'
    assert 'explorer.handle(appCtx, e)' in m and 'launcher.handle(appCtx, e)' in m
    assert 'physicalRecovery' in m and 'KEY_DOWN' in m
    g=(ROOT/'src/apps/LauncherGrid.cpp').read_text()
    for s in ['"WiFi","Bluetooth","Music"','"File mgr","Gallery","Internet"','"Shell","Recovery","Settings"','"Themes","Apps","Library"']:
        assert s in g, s
    assert 'ctx.ui.gridItem(old' in g and 'ctx.ui.gridItem(index' in g, 'Only dirty grid cells may redraw'
    assert 'return ScreenId::Explorer' in g, 'optional explorer accessible'
    config=(ROOT/'include/BoardConfig.h').read_text()
    for name,pin in {'TFT_LEDK_PIN':39,'TFT_DC_PIN':47,'TFT_CS_PIN':14,'TFT_SCL_PIN':48,'TFT_SDA_PIN':12,'TFT_RST_PIN':3,'KEY_MENU':18,'KEY_UP':7,'KEY_A':15,'KEY_LEFT':45,'KEY_START':17,'KEY_RIGHT':6,'KEY_OPTION':8,'KEY_DOWN':46,'KEY_B':5,'KEY_SELECT':16,'SD_D3':10,'SD_CMD':11,'SD_CLK':13,'SD_D0':9}.items():
        assert re.search(r'\b'+name+r'\s*=\s*'+str(pin)+r'\s*;',config)
    assert re.search(r'SCREEN_W\s*=\s*240',config) and re.search(r'SCREEN_H\s*=\s*320',config)
    assert 'using VqeafUI = SymbianUI;' in (ROOT/'src/core/SymbianUI.h').read_text()
    ext=(ROOT/'src/core/SymbianUI.cpp').read_text()
    assert 'No battery ADC in the confirmed BOM' in ext
    assert 'colors.bg = launcherSkin.listBg;' in ext and 'launcherSkin.footerFg' in ext
    assert 'prefixCap = 16 * 1024' in (ROOT/'src/services/ThemeFileService.cpp').read_text()
    assert (ROOT/'src/services/QeappSignature.cpp').exists(), 'Signed app implementation retained'
    print('PASS: 16 screen targets, 162 checked regions, Grid/Home actual routes, GPIO unchanged, theme/QEAPP baselines present')

def main():
    static()
    with tempfile.TemporaryDirectory(prefix='vqeaf-v22-') as tmp:
        test=Path(tmp)/'pixel'
        run('g++','-std=c++11','-Wall','-Wextra','-Werror','-I','src',
            'tools/vqeaf_host/test_pixel_atlas.cpp','src/core/UiScreenAtlas.cpp','-o',test)
        run(test)
        run('g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/theme_host',
            '-Isrc','-Isrc/services','-Isrc/core','tools/vqeaf_host/test_theme_studio.cpp',
            'src/services/ThemeFileService.cpp','-o',Path(tmp)/'theme')
        run(Path(tmp)/'theme','sd/Themes/vqeaf_night.vqeaf','sd/Themes/vqeaf_day.vqeaf','sd/System/Themes/vqeaf_reference_lime.vqeaf')
        run('g++','-std=c++11','-Wall','-Wextra','-Werror','-ffunction-sections',
            '-fdata-sections','-DVQEAF_INPUT_FAKE_CLOCK','-Itools/host_stubs','-Iinclude','-Isrc',
            'tools/vqeaf_host/test_input_t9.cpp','src/core/InputManager.cpp',
            'src/core/TextKeyboard.cpp','-Wl,--gc-sections','-o',Path(tmp)/'input')
        run(Path(tmp)/'input')
        run('g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/vqeaf_host/preview_stubs',
            '-Itools/host_stubs','-Iinclude','-Isrc','tools/vqeaf_host/test_launcher.cpp',
            'src/launcher/LauncherView.cpp','-o',Path(tmp)/'explorer')
        run(Path(tmp)/'explorer',Path(tmp)/'explorer.ppm')
    # The actual integrated sources, signed package gate, auto WiFi and board diag.
    for script in ('test_v14_build.py','test_v15_signature.py','test_v16_wifi.py',
                   'test_v17_auto_wifi.py','test_v18_core.py','test_v201_boarddiag.py'):
        print('TEST:',script)
        run(sys.executable,ROOT/'tools'/script)
    print('VQEAF 2.2 HOST TESTS PASS. ESP32 PlatformIO build and physical board PENDING.')
if __name__=='__main__':main()
