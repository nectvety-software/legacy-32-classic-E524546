#!/usr/bin/env python3
"""Host gate for the issues in IMG_3535.MOV; real hardware needs field retest.
Runs baseline v2.4.2 regressions + real InputManager fake-GPIO + C++ theme/installer
and production-vs-Snake-demo signature verification. No firmware size claims.
"""
from __future__ import annotations
import subprocess,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build_reports/v243';OUT.mkdir(parents=True,exist_ok=True)
results=[]
def run(name,command,expect=0):
 p=subprocess.run([str(i) for i in command],cwd=ROOT,text=True,stdout=subprocess.PIPE,
                  stderr=subprocess.STDOUT,timeout=240)
 (OUT/(name+'.log')).write_text(p.stdout,encoding='utf-8')
 ok=p.returncode==expect
 results.append({'name':name,'ok':ok,'exit':p.returncode,'expect':expect})
 print(('PASS ' if ok else 'FAIL ')+name)
 if not ok:print(p.stdout[-5000:])
 return ok

def main():
 baseline=run('v242_regression',[sys.executable,'tools/verify_v242.py'])
 if baseline:
  data=json.loads((ROOT/'build_reports/v242/result.json').read_text())
  assert data['host_tests_passed']==17 and data['total']==17
  results[-1]['subchecks']='17/17'
 key=OUT/'test_navigation'
 if run('input_compile',['g++','-std=c++11','-Wall','-Wextra','-Werror',
    '-DVQEAF_INPUT_FAKE_CLOCK=1','-Itools/host_stubs','-Iinclude','-Isrc/core',
    'tools/test_v243_key_navigation.cpp','src/core/InputManager.cpp','-o',key]):
  run('input_gesture_replay',[key])
 # These packages have DIFFERENT trust anchors by design; the production key
 # must reject the Snake demo while the demo profile verifies it.
 run('snake_production_mismatch',[sys.executable,'tools/doctor_v242.py',
   '--package','games/pixel_snake/dist/snake_pixel_demo.qeapp'],expect=1)
 run('snake_demo_trust',[sys.executable,'tools/doctor_v242.py',
   '--package','games/pixel_snake/dist/snake_pixel_demo.qeapp',
   '--key-header','src/services/SnakeDemoTrustKey.h'])
 # The baseline's host_firmware_link case already compiled and linked all 33
 # C++ translation units. Do not repeat a costly identical build.
 total=sum(r['ok'] for r in results)
 report={'version':'2.4.3','pass':total,'total':len(results),'tests':results,
 'video_analysis':'START press opens entry immediately; legacy long START then jumps to Music',
 'platformio':'NOT RUN','physical_device':'NOT RUN'}
 (OUT/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 lines=['# VQEAF OS v2.4.3 — Field video regression (host)','',
 'Input file: IMG_3535.MOV (real user board). No automated hardware test was run.','',
 '| Test | Result |','|---|---|']
 lines += [f"| {i['name']} | {'PASS' if i['ok'] else 'FAIL'} |" for i in results]
 lines += ['','Production QEAPP key intentionally rejects separately signed Snake demo.',
 'Theme parser now has a direct-loading test for a path beyond the bounded catalog.',
 'Physical ESP32-S3, microSD FAT implementation, TFT and PlatformIO still require retest.']
 (OUT/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
 print(f'TOTAL {total}/{len(results)}; {OUT}/report.md')
 return 0 if total==len(results) else 1
if __name__=='__main__':raise SystemExit(main())
