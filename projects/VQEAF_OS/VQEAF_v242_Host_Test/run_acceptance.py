#!/usr/bin/env python3
"""VQEAF OS v2.4.2 integration verification and HOST screenshot capture.
NOT an ESP32-S3 upload, device screenshot, live HTTP session or physical SD check.
Run from anywhere: python3 run_acceptance.py
Needs g++, libcrypto-dev/OpenSSL, Python cryptography + Pillow.
"""
from pathlib import Path
import json, shutil, subprocess, datetime, sys, hashlib, tempfile, os
from PIL import Image,ImageDraw,ImageFont
BASE=Path(__file__).resolve().parent
R=Path(os.environ.get('VQEAF_SOURCE_DIR', str(BASE.parent/'VQEAF-OS'))).resolve()
if not (R/'src/services/AppInstallerService.cpp').is_file():
    raise SystemExit('Set VQEAF_SOURCE_DIR to your extracted VQEAF-OS source folder')
OUT=BASE/'screenshots'; OUT.mkdir(parents=True,exist_ok=True)
LOG=BASE/'logs';LOG.mkdir(parents=True,exist_ok=True)
TEST=BASE/'tests'

def run(name,cmd,expect=0):
    r=subprocess.run([str(x) for x in cmd],cwd=R,text=True,capture_output=True,timeout=170)
    data='$ '+' '.join(map(str,cmd))+'\n'+r.stdout+r.stderr+'\n[exit] '+str(r.returncode)+'\n'
    (LOG/(name+'.log')).write_text(data,encoding='utf-8')
    if r.returncode!=expect:
        raise RuntimeError(f'{name}: return {r.returncode}, expected {expect}\n'+data[-1800:])
    print('PASS',name)
    return r.stdout

def bin_build(name,extra_src,extra_flags,host_include,files,libs=()):
    dst=BASE/name
    cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',*extra_flags,
         '-I'+str(TEST),'-I'+str(R/host_include),*(['-Itools/host_stubs'] if 'v19_browser' in host_include or 'vqeaf_host' in host_include else []),'-Isrc','-Iinclude',
         TEST/extra_src,*files,*libs,'-o',dst]
    run(name+'_compile',cmd)
    return dst

# Production packages: real AppInstallerService plus real QEAPP/2 ECDSA verifier.
app=bin_build('actual_app_install','install_capture.cpp', ['-DQEAPP_HOST_OPENSSL'],
    'tools/qeapp_host', [R/'src/services'/n for n in ['QeappFormat.cpp','QeappVersion.cpp','QeappSignature.cpp','AppInstallerService.cpp']],['-lcrypto'])
sd=BASE/'emulated_sd'
if sd.exists(): shutil.rmtree(sd)
run('production_package_install', [app,sd,
     R/'sd/System/Apps/Inbox/welcome.qeapp',R/'sd/System/Apps/Inbox/help_site.qeapp'])
assert (sd/'System/Apps/Installed/welcome/receipt.bin').is_file()
assert (sd/'System/Apps/Installed/help_site/receipt.bin').is_file()
# microSD theme folder gets an ACTUAL .vqeaf copied from project fixtures.
theme_folder=sd/'System/Themes';theme_folder.mkdir(parents=True,exist_ok=True)
shutil.copyfile(R/'sd/System/Themes/s60_green.vqeaf',theme_folder/'s60_green.vqeaf')
theme=bin_build('actual_theme_parser','theme_capture.cpp',['-Wno-error=return-type'],
    'tools/theme_host',[R/'src/services/ThemeFileService.cpp'])
run('theme_scan_apply_validation',[theme,theme_folder/'s60_green.vqeaf',BASE/'theme_palette.json'])
# Test the real SettingsStore persistence interface with host-only NVS shim.
persist=bin_build('actual_theme_persistence','persist_test.cpp', ['-I'+str(TEST/'persist_stubs')],
    'tools/host_stubs', [R/'src/services/SettingsStore.cpp'])
run('theme_persistence_reboot', [persist])
theme_values=json.loads((BASE/'theme_palette.json').read_text())['colors']
assert len(theme_values)==13
# BrowserCore HTTP is deliberately deterministic/isolated: no live TLS or WiFi.
browser=bin_build('actual_browser_parse','browser_capture.cpp',['-Wno-error=return-type'],
    'tools/v19_browser_host', [R/'tools/v19_browser_host/extra.cpp',R/'src/services/BrowserService.cpp',R/'src/services/TrustedTls.cpp'])
run('browser_html_and_link',[browser,BASE/'browser_page.json'])
ui=bin_build('actual_ui_raster','ui_capture.cpp',['-fpermissive'],
    'tools/vqeaf_host/reference_stubs', [R/'src/core/SymbianUI.cpp',R/'src/core/VqeafIconRenderer.cpp',R/'tools/host_stubs/host_globals.cpp'])
run('rgb565_ui_render',[ui,OUT,*theme_values,'1'])
# Actual pixel framebuffer output from production SymbianUI -> PPM -> PNG.
for p in OUT.glob('*.ppm'):
    Image.open(p).convert('RGB').save(p.with_suffix('.png'))
    p.unlink()
# 5 verified states, from actual code paths; C++ renderer with host TTF substitute fonts.
frames=sorted(p for p in OUT.glob('0*.png') if p.name!='00_contact_sheet.png')
assert len(frames)==5 and all(Image.open(p).size==(240,320) for p in frames)
style_path='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
font=ImageFont.truetype(style_path,17)
small=ImageFont.truetype(style_path,13)
canvas=Image.new('RGB',(1330,520),(29,42,36));draw=ImageDraw.Draw(canvas)
labels=['CÀI ĐẶT .QEAPP','ỨNG DỤNG ĐÃ CÀI','ÁP DỤNG .VQEAF','MENU SAU ĐỔI THEME','TRÌNH DUYỆT HTML']
for i,(p,label) in enumerate(zip(frames,labels)):
    im=Image.open(p).convert('RGB')
    x=15+i*263;y=56
    canvas.paste(im,(x,y));draw.text((x,y-29),label,font=small,fill=(229,249,229))
    draw.text((x,y+329),'240 x 320 / Host C++',font=small,fill=(186,207,194))
draw.text((15,8),'VQEAF OS v2.4.2 - KIEM THU HOST: APP / THEME / BROWSER',font=font,fill=(238,255,237))
draw.text((15,415),'Ảnh framebuffer từ SymbianUI.cpp, dữ liệu kiểm thử từ các lõi C++ thực.',font=font,fill=(233,246,239))
draw.text((15,440),'Không phải ảnh chụp ESP32-S3. HTTP và thẻ SD được mô phỏng trên PC.',font=small,fill=(193,207,195))
canvas.save(OUT/'00_contact_sheet.png')
report={
    'app_real_cpp_install':True,'installed':[{'id':'welcome','receipt':True},{'id':'help_site','receipt':True}],
    'theme_real_cpp_validated':True,'theme_persistence_after_restart':True,'theme':theme_values,
    'browser_real_cpp_html_parsed_mock_http':True,
    'frames_from_production_cpp_renderer':len(frames),
    'physical_esp32':False,'real_micro_sd':False,'live_internet_tls':False,
}
(BASE/'acceptance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('RESULT: host installation, theme, browser and C++ screenshots PASS')
print('No real board attached; NO ESP32-S3 hardware screenshot or physical build is claimed.')
