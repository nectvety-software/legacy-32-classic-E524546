#!/usr/bin/env python3
"""Run on-host reproducibility, CRC goldens, device harness and OS UI parity.
Passing this suite does NOT establish physical ESP32 execution.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from datetime import datetime, timezone
R=Path(__file__).resolve().parents[1]
OUT=R/'build_reports'/'icon_device_v236'
OUT.mkdir(parents=True,exist_ok=True)

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

# Fixed independent reference hashes from the archived v234 baseline.
# A change to the reference baseline MUST trigger a failure, not rebaseline.
REFERENCE_HASHES={
 'VqeafIconData.h':'c06316353f707a22c73e1f93b4feb60e8fb633b7adb3de9888f60f3de4c89f4c',
 'VqeafIconRenderer.cpp':'7e353f99ee64989e7d922f1f18f7eec3549b5417358635aaa5acc1061c10a0d1',
 'VqeafIconRenderer.h':'83bbd8bd7d9b044f3102783d91d1aa19b1cb305a1af8b41cbbb72b63ec3cc77d',
}
checks=[]
def record(name,ok,output):
    (OUT/(name+'.log')).write_text(output,encoding='utf-8')
    checks.append(dict(name=name,status='PASS' if ok else 'FAIL'))
    print(('PASS' if ok else 'FAIL')+': '+name)
    if not ok:raise RuntimeError(name+': '+output[-1800:])

def run(name,command):
    p=subprocess.run(command,cwd=R,capture_output=True,text=True,timeout=120)
    record(name,p.returncode==0,p.stdout+'\n'+p.stderr)

try:
    hashes={}
    for filename,expected in REFERENCE_HASHES.items():
        found=sha(R/'docs/verification/v234_baseline'/filename)
        hashes[filename]=found
        assert found==expected,(filename,found,expected)
    record('frozen_v234_reference',True,json.dumps(hashes,indent=2))
    run('baseline_vs_optimized_cpp',[sys.executable,'tools/test_lossless_icons_v235.py'])
    run('board_layout',[sys.executable,'tools/check_board_config.py'])
    # Build EXACT same device runner/decoder with host TFT stub and Arduino shim.
    exe=OUT/'test_on_device_harness_host'
    run('compile_device_harness_host',[
      'g++','-std=c++11','-O2','-Wall','-Wextra','-Werror',
      '-D','VQEAF_ICON_SELFTEST=1',
      '-Itools/icon_device_host','-Itools/icon_host','-Isrc/core',
      'src/core/VqeafIconRenderer.cpp','src/core/VqeafIconSelfTest.cpp',
      'tools/icon_device_host/test_icon_selftest_host.cpp','-o',str(exe)])
    run('run_device_harness_host',[str(exe)])
    text=(OUT/'run_device_harness_host.log').read_text()
    assert '[ICONTEST] SUMMARY passed=192 expected=192 errors=0 result=PASS' in text
    # Semantic check: required PlatformIO variants and only the baseline codec flag.
    ini=(R/'platformio.ini').read_text()
    for env in ('vqeaf_size_baseline','vqeaf_size_optimized','vqeaf_icon_selftest'):
        assert f'[env:{env}]' in ini
    assert 'VQEAF_ICON_BASELINE=1' in ini and 'VQEAF_ICON_SELFTEST=1' in ini
    assert '#include "../../docs/verification/v234_baseline/VqeafIconRenderer.cpp"' in (R/'src/core/VqeafIconRenderer.cpp').read_text()
    record('pio_variants',True,'All three variants exist. Reference compiled only for baseline size env.')
except Exception as ex:
    print('FAIL:',ex,file=sys.stderr)
finally:
    report={'utc':datetime.now(timezone.utc).isoformat(timespec='seconds'),
      'board':'ESP32-S3 N16R8',
      'on_device_execution':'NOT PERFORMED',
      'whole_firmware_size':'NOT MEASURED (run tools/measure_firmware_icon_impact.py on PlatformIO-equipped PC)',
      'checks':checks,'passed':len(checks)==6 and all(c['status']=='PASS' for c in checks)}
    (OUT/'host_verification.json').write_text(json.dumps(report,indent=2)+'\n')
    (OUT/'host_verification.md').write_text('# VQEAF OS v2.3.6 host verification\n\n'+
      '\n'.join('- '+c['name']+': '+c['status'] for c in checks)+
      '\n\nBoard execution: NOT PERFORMED. Full-firmware target size: NOT MEASURED.\n')
    print('Written',OUT/'host_verification.md')
sys.exit(0 if len(checks)==6 and all(c['status']=='PASS' for c in checks) else 1)
