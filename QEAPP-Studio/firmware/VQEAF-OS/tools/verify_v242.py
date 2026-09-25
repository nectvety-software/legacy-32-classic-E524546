#!/usr/bin/env python3
"""Host-only QEAPP/browser/theme regression gate; writes an exact report.
Physical WiFi, HTTPS certificate paths and microSD write health are NOT tested.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
OUT=R/'build_reports/v242'
OUT.mkdir(parents=True,exist_ok=True)


def check(name,args,expect=0):
    p=subprocess.run([str(x) for x in args],cwd=R,capture_output=True,text=True,timeout=180)
    content=p.stdout+'\n'+p.stderr
    (OUT/(name+'.log')).write_text(content,encoding='utf-8')
    ok=(p.returncode==expect)
    print(('PASS' if ok else 'FAIL')+': '+name+' (exit='+str(p.returncode)+')',flush=True)
    if not ok:print(content[-3000:])
    return dict(name=name,ok=ok,exit_code=p.returncode)

def main():
 results=[]
 with tempfile.TemporaryDirectory(prefix='vqeaf242-') as td:
    t=Path(td)
    build=[
      ('theme_runtime',['g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/theme_host','-Isrc','-Isrc/services','-Isrc/core','tools/theme_host/test_theme_runtime.cpp','src/services/ThemeFileService.cpp','-o',t/'theme'],
       [t/'theme','sd/Themes/amoled_red.vqeaf','sd/Themes/s60_green.vqeaf']),
      ('theme_studio',['g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/theme_host','-Isrc','-Isrc/services','-Isrc/core','tools/vqeaf_host/test_theme_studio.cpp','src/services/ThemeFileService.cpp','-o',t/'studio'],
       [t/'studio','sd/Themes/vqeaf_night.vqeaf','sd/Themes/vqeaf_day.vqeaf','sd/System/Themes/vqeaf_reference_lime.vqeaf']),
      ('http_chunk_fuzz',['g++','-std=c++11','-Wall','-Wextra','-Werror','-Iinclude','-Isrc','tools/test_chunk_decoder_v242.cpp','-o',t/'chunk'],[t/'chunk']),
      ('browser_network',['g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/v19_browser_host','-Itools/host_stubs','-Iinclude','-Isrc',
            'tools/v19_browser_host/test_browser_v242.cpp','tools/v19_browser_host/extra.cpp','src/services/BrowserService.cpp','src/services/TrustedTls.cpp','-o',t/'browser'],[t/'browser']),
      ('browser_v19_regression',['g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/v19_browser_host','-Itools/host_stubs','-Iinclude','-Isrc',
            'tools/v19_browser_host/test_browser_http.cpp','tools/v19_browser_host/extra.cpp','src/services/BrowserService.cpp','src/services/TrustedTls.cpp','-o',t/'browserold'],[t/'browserold'])
    ]
    for name,buildcmd,runcmd in build:
       result=check(name+'_compile',buildcmd)
       results.append(result)
       if result['ok']:results.append(check(name+'_run',runcmd))
    tests=[('signed_installer', [sys.executable,'tools/test_v24_app_manager.py']),
           ('production_packages',[sys.executable,'tools/doctor_v242.py']),
           ('demo_package',[sys.executable,'tools/doctor_v242.py','--package','games/pixel_snake/dist/snake_pixel_demo.qeapp',
                             '--key-header','src/services/SnakeDemoTrustKey.h']),
           ('host_firmware_link',[sys.executable,'tools/test_v14_build.py']),
           ('signature_regression',[sys.executable,'tools/test_v15_signature.py'])]
    for name,cmd in tests:results.append(check(name,cmd))
 # Boot/field hooks are checked structurally; target compilation remains pending.
 for key,filename in [('corediag','src/services/ShellService.cpp'),('[VQEAF][CORE]','src/main.cpp')]:
     results.append(dict(name='hook_'+filename,ok=key in (R/filename).read_text(),exit_code=0))
 summary={'host_tests_passed':sum(x['ok'] for x in results),'total':len(results),
     'target_platformio_build':'NOT RUN','physical_esp32_test':'NOT RUN',
     'live_https':'NOT RUN','cases':results}
 (OUT/'result.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
 md='# VQEAF OS v2.4.2 — Host validation\n\n| Case | Result |\n|---|---|\n'
 md+=''.join('| '+x['name']+' | '+('PASS' if x['ok'] else 'FAIL')+' |\n' for x in results)
 md+='\n**Firmware PlatformIO, real WiFi/TLS, microSD and LCD:** NOT RUN.\n'
 (OUT/'report.md').write_text(md,encoding='utf-8')
 print('TOTAL',summary['host_tests_passed'],'/',summary['total'],'report:',OUT/'report.md')
 return int(summary['host_tests_passed']!=summary['total'])
if __name__=='__main__':raise SystemExit(main())
