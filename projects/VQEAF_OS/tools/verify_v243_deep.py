#!/usr/bin/env python3
"""OS v2.4.3 extended HOST acceptance; never implies actual ESP32-S3 execution.
Runs active contemporary suites, sanitizer builds & deterministic mutation tests.
Legacy static version-string scripts test_v12/test_v13/test_v21 are intentionally
excluded; their preserved expectations predate the Home/keyboard redesign.
"""
import argparse, json, subprocess, sys, tempfile, os, shutil, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build_reports/v243_deep'
OUT.mkdir(parents=True,exist_ok=True)
results=[]

def execute(name,cmd,timeout=240,env=None):
    start=time.monotonic()
    try:
        p=subprocess.run(list(map(str,cmd)),cwd=ROOT,text=True,capture_output=True,
                         timeout=timeout,env=env)
        ok=p.returncode==0
        log=p.stdout+'\n'+p.stderr
        code=p.returncode
    except subprocess.TimeoutExpired as e:
        ok=False;code=124;log=str(e)
    (OUT/(name+'.log')).write_text(log,encoding='utf-8',errors='replace')
    result={'name':name,'status':'PASS' if ok else 'FAIL','code':code,
            'seconds':round(time.monotonic()-start,2)}
    results.append(result)
    print(result['status']+': '+name+' ('+str(result['seconds'])+'s)',flush=True)
    if not ok: print(log[-1700:],flush=True)
    return ok

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--quick',action='store_true',help='skip expensive redundant UI/icon/TLS suites')
    ap.add_argument('--no-sanitizers',action='store_true',help='skip ASan/UBSan diagnostics')
    args=ap.parse_args()
    base=[
      ('current_v243_regressions',[sys.executable,'tools/verify_v243.py']),
      ('full_qeapp_manager',[sys.executable,'tools/test_v24_app_manager.py']),
      ('pixel_snake',[sys.executable,'tools/test_pixel_snake.py']),
    ]
    if not args.quick:
      base += [
       ('tls_storage_history',[sys.executable,'tools/test_v20_storage_tls.py']),
       ('modern_home_menu_t9',[sys.executable,'tools/test_ui_v23.py']),
       ('pixel_icon_source',[sys.executable,'tools/test_vqeaf_icons.py']),
       ('pixel_icon_crc',[sys.executable,'tools/verify_icon_device_v236.py']),
       ('offline_dry_run',[sys.executable,'-m','unittest','tools.test_offline_build','-k','dry_run','-v']),
      ]
    for name,cmd in base:execute(name,cmd,timeout=250)
    if not args.no_sanitizers:
      if not shutil.which('g++'):
        print('SKIP sanitizers: g++ unavailable',flush=True)
      else:
        sflags=['-O1','-g','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror']
        cxx='g++'
        cfg=[
        ('theme_mutation_ubsan',
         [cxx,'-std=c++11',*sflags,'-Wno-error=return-type','-fsanitize=undefined','-fno-sanitize-recover=undefined',
           '-Itools/theme_host','-Isrc','-Isrc/services','-Isrc/core','tools/deep_host/theme_mutation_ubsan.cpp',
           'src/services/ThemeFileService.cpp'], []),
        ('browser_stress_asan',
         [cxx,'-std=c++11',*sflags,'-fsanitize=address,undefined','-fno-sanitize-recover=all',
           '-Itools/v19_browser_host','-Itools/host_stubs','-Iinclude','-Isrc','tools/deep_host/browser_stress_asan.cpp',
           'tools/v19_browser_host/extra.cpp','src/services/BrowserService.cpp','src/services/TrustedTls.cpp'], []),
        ('qeapp_mutation_asan',
         [cxx,'-std=c++17',*sflags,'-Wno-deprecated-declarations','-DQEAPP_HOST_OPENSSL',
           '-fsanitize=address,undefined','-fno-sanitize-recover=all',
           '-Itools/qeapp_host','-Isrc/services','tools/deep_host/qeapp_mutation_asan.cpp',
           'src/services/QeappFormat.cpp','src/services/QeappVersion.cpp',
           'src/services/QeappSignature.cpp','src/services/AppInstallerService.cpp','-lcrypto'], []),
        ]
        with tempfile.TemporaryDirectory(prefix='vqeaf243-deep-') as d:
          d=Path(d)
          for name,compile_cmd,_ in cfg:
            exe=d/name
            # Linker flags must be after sources for systems using --as-needed.
            suffix=[]
            if compile_cmd[-1]=='-lcrypto':
              suffix=['-lcrypto'];compile_cmd=compile_cmd[:-1]
            if not execute('build_'+name,compile_cmd+['-o',exe]+suffix,timeout=150):continue
            if name=='theme_mutation_ubsan': cmd=[exe]
            elif name=='browser_stress_asan':cmd=[exe]
            else:
              sd=d/'qeapp_card';sd.mkdir(exist_ok=True)
              cmd=[exe,sd,'sd/System/Apps/Inbox/welcome.qeapp']
            execute('run_'+name,cmd,timeout=90,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
    passed=sum(x['status']=='PASS' for x in results)
    report={'scope':'host_only','version':'v2.4.3 patched acceptance candidate',
       'passed':passed,'total':len(results),'status':'PASS' if passed==len(results) else 'FAIL',
       'platformio_esp32_target':'NOT_RUN','physical_board':'NOT_RUN',
       'live_sd_wifi_https_tft_psram':'NOT_RUN','cases':results}
    (OUT/'results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    lines=['# VQEAF OS v2.4.3 extended host verification','','| Check | Status | Time (s) |','|---|---|---:|']
    lines += [f"| `{x['name']}` | {x['status']} | {x['seconds']} |" for x in results]
    lines += ['','**Not tested:** PlatformIO ESP32 cross-build, real microSD/PSRAM/LCD/WiFi/Bluetooth/HTTPS or physical keypad.','']
    (OUT/'report.md').write_text('\n'.join(lines),encoding='utf-8')
    print('FINAL',passed,'/',len(results),'report',OUT/'report.md',flush=True)
    return 0 if passed==len(results) else 1
if __name__=='__main__':raise SystemExit(main())
