#!/usr/bin/env python3
"""Reproducible local board build / log capture, never reports host as target.

Usage: python tools/build_pio.py             # offline checks + host + PlatformIO
       python tools/build_pio.py --host-only # no target attempt
       python tools/build_pio.py --target-only # offline checks + PlatformIO (CI)
       python tools/build_pio.py --host-only --full-host # exhaustive legacy regression
"""
import argparse
import datetime
import json
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parent.parent
REPORT=ROOT/'build_reports'


def run(args, log_path):
    p=subprocess.run(args,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                     text=True,errors='replace')
    log_path.write_text('$ '+' '.join(map(str,args))+'\n'+p.stdout+'\nexit_code='+str(p.returncode)+'\n',encoding='utf-8')
    print(f"{'PASS' if p.returncode==0 else 'FAIL'} {' '.join(map(str,args))}: {log_path.relative_to(ROOT)}")
    if p.returncode: print(p.stdout[-3000:])
    return p.returncode,p.stdout


def main():
    a=argparse.ArgumentParser()
    a.add_argument('--host-only',action='store_true')
    a.add_argument('--target-only',action='store_true')
    a.add_argument('--full-host',action='store_true',help='Run legacy full GUI regression after fast host smoke')
    opts=a.parse_args()
    if opts.host_only and opts.target_only:a.error('Choose at most one mode')
    REPORT.mkdir(exist_ok=True)
    status={'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'board':'vqeaf_s3_n16r8','environment':'vqeaf_os',
            'offline_preflight':'NOT_RUN','build_guards':'NOT_RUN','host_cpp_link':'NOT_RUN','host_icon_ui':'NOT_RUN','host_full_regression':'NOT_RUN',
            'platformio_target':'NOT_RUN','firmware_bin':'NOT_GENERATED'}
    try:
        status['offline_preflight']='PASS' if run([sys.executable,'tools/check_board_config.py'],REPORT/'preflight.log')[0]==0 else 'FAIL'
        if status['offline_preflight']=='FAIL':return 1
        # Native g++ is optional on Windows. Missing host tools must never prevent
        # a real PlatformIO cross-compile from running; target-only also works.
        gxx=shutil.which('g++')
        if gxx:
            status['build_guards']='PASS' if run([sys.executable,'tools/test_build_sanity.py'],REPORT/'build_guards.log')[0]==0 else 'FAIL'
            if status['build_guards']=='FAIL':return 1
        else:
            status['build_guards']='SKIPPED_NO_GXX'
            print('SKIP C++ build-guard host test: native g++ not installed')
            if opts.host_only:
                return 2
        if not opts.target_only:
            if gxx:
                status['host_cpp_link']='PASS' if run([sys.executable,'tools/test_v14_build.py'],REPORT/'host_cpp_link.log')[0]==0 else 'FAIL'
                if status['host_cpp_link']=='FAIL':return 1
            else:
                status['host_cpp_link']='SKIPPED_NO_GXX'
            if gxx and importlib.util.find_spec('PIL'):
                status['host_icon_ui']='PASS' if run([sys.executable,'tools/test_board_host_smoke.py'],REPORT/'host_icon_ui.log')[0]==0 else 'FAIL'
                if status['host_icon_ui']=='FAIL':return 1
            else:
                status['host_icon_ui']='SKIPPED_NO_GXX_OR_PILLOW'
                print('SKIP icon/GUI host test: native g++ and Pillow required')
                if opts.host_only:return 2
            if opts.full_host and status['host_icon_ui']=='PASS':
                status['host_full_regression']='PASS' if run([sys.executable,'tools/test_icon_integration_v232.py'],REPORT/'host_full_regression.log')[0]==0 else 'FAIL'
                if status['host_full_regression']=='FAIL':return 1
        if not opts.host_only:
            pio=shutil.which('pio') or shutil.which('platformio')
            if not pio:
                status['platformio_target']='BLOCKED_MISSING_PIO'
                (REPORT/'platformio_build.log').write_text('NOT_RUN: PlatformIO executable not installed in PATH.\n'
                    'Install: python -m pip install platformio\nRun: python tools/build_pio.py\n',encoding='utf-8')
                print('BLOCKED: PlatformIO not installed. No .bin can be claimed.')
                return 2
            status['platformio_version']='UNKNOWN'
            code,out=run([pio,'--version'],REPORT/'platformio_version.log')
            if code:status['platformio_target']='BLOCKED_PIO_VERSION';return 2
            status['platformio_version']=out.strip().splitlines()[-1] if out.strip() else 'UNKNOWN'
            code,_=run([pio,'run','-e','vqeaf_os','-v'],REPORT/'platformio_build.log')
            status['platformio_target']='PASS' if code==0 else 'FAIL'
            if code:return code
            firmware=ROOT/'.pio/build/vqeaf_os/firmware.bin'
            if not firmware.is_file() or not firmware.stat().st_size:
                status['firmware_bin']='MISSING_AFTER_BUILD'
                status['platformio_target']='FAIL_NO_BIN'
                return 1
            status['firmware_bin']='GENERATED'
            status['firmware_bytes']=firmware.stat().st_size
        return 0
    finally:
        (REPORT/'build_status.json').write_text(json.dumps(status,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('Saved build status:',(REPORT/'build_status.json').relative_to(ROOT))


if __name__=='__main__':
    sys.exit(main())
