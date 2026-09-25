#!/usr/bin/env python3
"""Host acceptance gate for VQEAF OS 2.5.1; DOES NOT replace ESP32-S3 device testing."""
import argparse
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path
R=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--full',action='store_true',help='Additional existing app manager and v2.4.3 full regressions')
    opt=parser.parse_args()
    out=R/'build_reports/v251';out.mkdir(parents=True,exist_ok=True)
    basic = [
      ('compile_app_icon_blit',['g++','-std=c++11','-Wall','-Wextra','-Werror','-Itools/v251_host',
         'tools/v251_host/test_qeapp_icon_blit.cpp','-o',str(out/'icon_blit_test')]),
      ('app_icon_blit_pixel_endianness',[str(out/'icon_blit_test')]),
      ('compile_fixed_memory_frame_metrics',['g++','-std=c++11','-Wall','-Wextra','-Werror',
         'tools/v251_host/test_frame_metrics.cpp','-o',str(out/'frame_metrics_test')]),
      ('frame_metrics_wrap_and_percentile',[str(out/'frame_metrics_test')]),
      ('serial_parser_synthetic',[sys.executable,'tools/test_v251_perf_parser.py']),
      ('signed_qeapp_installer_icon_tamper',[sys.executable,'tools/test_v15_signature.py']),
      ('full_firmware_link_with_perf_macros',[sys.executable,'tools/test_perf_diag_host.py']),
      ('v250_rgb565_ui_reset_and_board_regression',[sys.executable,'tools/verify_v250.py','--quick']),
    ]
    if opt.full:
      basic += [
       ('app_manager_update_rollback',[sys.executable,'tools/test_v24_app_manager.py']),
       ('v243_deep_active_regressions',[sys.executable,'tools/verify_v243_deep.py','--quick','--no-sanitizers']),
      ]
    if not shutil.which('g++'): raise SystemExit('g++ missing; install host compiler or use another machine')
    stats=[]
    for label,cmd in basic:
        tick=time.monotonic()
        try:
            proc=subprocess.run(cmd,cwd=R,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=185)
            status='PASS' if proc.returncode==0 else 'FAIL';text=proc.stdout
            code=proc.returncode
        except subprocess.TimeoutExpired as e:
            status='TIMEOUT';text=str(e);code=124
        (out/f'{label}.log').write_text(text,encoding='utf8',errors='replace')
        stats.append(dict(case=label,status=status,exit=code,seconds=round(time.monotonic()-tick,2)))
        print(status,label,stats[-1]['seconds'],'s',flush=True)
        if status!='PASS':
            print(text[-2200:],flush=True)
            break
    good=len(stats)==len(basic) and all(z['status']=='PASS' for z in stats)
    result={'version':'VQEAF OS 2.5.1','scope':'HOST_ONLY', 'overall':'PASS' if good else 'FAIL',
       'platformio_target_build':'NOT_RUN','physical_esp32_s3':'NOT_CONNECTED',
       'actual_esp32_fps':'NOT_MEASURED','actual_input_to_photon_latency':'NOT_MEASURED',
       'note':'Host C++ mocks cannot model physical SPI throughput, power, SD timings, or LCD scanout',
       'checks':stats}
    (out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    lines=['# VQEAF OS v2.5.1 — Host acceptance','','**Scope: PC tests / firmware shims.**','',
        '| Test | Result | Time |','|---|---|---:|']
    lines += [f"| {x['case']} | {x['status']} | {x['seconds']:.2f} s |" for x in stats]
    lines += ['','**Overall:** '+result['overall'],'',
      'Device FPS, physical keypad-to-pixel latency, and target PlatformIO build: **not measured here**.','']
    (out/'report.md').write_text('\n'.join(lines),encoding='utf8')
    return 0 if good else 1
if __name__=='__main__':raise SystemExit(main())
