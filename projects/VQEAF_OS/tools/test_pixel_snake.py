#!/usr/bin/env python3
"""Host-only game + actual QEAPP/2 signature integration test. No ESP32 cross-build."""
import subprocess,sys,tempfile,shutil,hashlib,json,datetime
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
BASE=ROOT/'build_reports/pixel_snake'
BASE.mkdir(parents=True,exist_ok=True)

def run(cmd,log):
    r=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True)
    (BASE/log).write_text(r.stdout+r.stderr,encoding='utf-8')
    if r.returncode:
        print(f'FAIL {log}:\n'+(r.stdout+r.stderr)[-3500:]);raise SystemExit(r.returncode)
    print('PASS:',log)

with tempfile.TemporaryDirectory(prefix='vqeaf-snake-host-') as t:
    binary=Path(t)/'test-snake'
    run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-DPIXEL_SNAKE_TEST','-Isrc/apps',
         'tools/host_pixel_snake/test_snake.cpp','src/apps/PixelSnakeLogic.cpp',
         'src/apps/PixelSnakeConfig.cpp','src/apps/PixelSnakeRender.cpp','-o',binary],'compile_game.log')
    run([binary,BASE/'screen_ready.ppm',BASE/'screen_playing.ppm'],'test_game.log')
    signed=Path(t)/'test-signed'
    run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
         '-DQEAPP_HOST_OPENSSL','-DVQEAF_SNAKE_DEMO_KEY=1',
         '-Itools/qeapp_host','-Isrc/services','src/services/QeappFormat.cpp',
         'src/services/QeappVersion.cpp','src/services/QeappSignature.cpp',
         'src/services/AppInstallerService.cpp','tools/host_pixel_snake/test_snake_signed.cpp',
         '-lcrypto','-o',signed],'compile_signed_installer.log')
    run([signed,Path(t)/'sd','games/pixel_snake/dist/snake_pixel_demo.qeapp'],'test_signed_installer.log')
    foreign=Path(t)/'test-foreign'
    run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
         '-DQEAPP_HOST_OPENSSL',
         '-Itools/qeapp_host','-Isrc/services','src/services/QeappFormat.cpp',
         'src/services/QeappVersion.cpp','src/services/QeappSignature.cpp',
         'src/services/AppInstallerService.cpp','tools/host_pixel_snake/test_snake_foreign.cpp',
         '-lcrypto','-o',foreign],'compile_default_pin.log')
    run([foreign,Path(t)/'foreign_sd','games/pixel_snake/dist/snake_pixel_demo.qeapp'],'test_default_pin_rejection.log')
    from PIL import Image
    for name in ['screen_ready','screen_playing']:
        im=Image.open(BASE/(name+'.ppm'))
        assert im.size==(240,320)
        im.save(BASE/(name+'.png'))
        im.resize((720,960),Image.Resampling.NEAREST).save(BASE/(name+'_3x.png'))
report={
  'test':'Pixel Snake v1.0 / QEAPP/2',
  'generated_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
  'game_host_cxx11':'PASS',
  'actual_installer_openssl':'PASS',
  'default_pin_isolation':'PASS',
  'demo_package_sha256':hashlib.sha256((ROOT/'games/pixel_snake/dist/snake_pixel_demo.qeapp').read_bytes()).hexdigest(),
  'preview_resolution':'240x320',
  'esp32_platformio_build':'NOT_RUN',
  'physical_device':'NOT_TESTED',
}
(BASE/'result.json').write_text(json.dumps(report,indent=2)+'\n')
(BASE/'report.md').write_text('# Pixel Snake verification\n\n'+
 'PASS: GNU++11 game/renderer + frame geometry; signed QEAPP/2 inspected and installed through actual firmware verifier on host.\n\n'+
 '**PlatformIO ESP32-S3:** NOT RUN. **Hardware:** NOT TESTED.\n\n'+
 'Demo package SHA-256: `'+report['demo_package_sha256']+'`\n')
print('Host integration tests: PASS. PlatformIO build not run.')
