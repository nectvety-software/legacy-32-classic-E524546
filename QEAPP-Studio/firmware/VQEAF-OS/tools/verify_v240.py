#!/usr/bin/env python3
"""Fast release gate. Host tests are not an ESP32 PlatformIO target build."""
from pathlib import Path
import subprocess
import sys
import datetime
ROOT=Path(__file__).resolve().parent.parent
RESULT=ROOT/'build_reports/v240'
RESULT.mkdir(parents=True,exist_ok=True)
CASES=[
    'check_board_config.py',
    'test_v14_wiring.py',
    'test_v14_build.py',
    'test_v15_signature.py',
    'test_v24_app_manager.py',
    'test_pixel_icons_v234.py',
    'test_grid_nav.py',
]

def main():
    results=[]
    for name in CASES:
        run=subprocess.run([sys.executable,str(ROOT/'tools'/name)],cwd=ROOT,
            text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
        (RESULT/(name+'.log')).write_text(run.stdout,encoding='utf-8')
        state='PASS' if run.returncode==0 else 'FAIL'
        print(f'{state}: {name} (exit={run.returncode})')
        results.append((name,state,run.returncode))
    report='''# VQEAF OS 2.4.0 — test report\n\nGenerated UTC: {}\n\n'''.format(datetime.datetime.now(datetime.timezone.utc).isoformat())
    report+='| Test | Status | Exit |\n|---|---|---:|\n'
    for name,state,code in results:report+=f'| `{name}` | {state} | {code} |\n'
    report+='''\n**PlatformIO ESP32-S3 cross-build:** NOT RUN by this script.\n
**Physical device installation:** NOT RUN.\n
**Firmware image (.bin):** NOT PRODUCED by these host-only tests.\n
Historical UI gate `test_v21.py` expects an obsolete literal code line;
`test_vqeaf_g3.py` expects an old unbundled PNG preview. These are not part
of this app-core release gate.\n'''
    (RESULT/'report.md').write_text(report,encoding='utf-8')
    print('Report:',RESULT/'report.md')
    return int(any(state!='PASS' for _,state,_ in results))
if __name__=='__main__':raise SystemExit(main())
