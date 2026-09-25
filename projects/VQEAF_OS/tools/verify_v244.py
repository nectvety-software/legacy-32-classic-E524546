#!/usr/bin/env python3
"""VQEAF v2.4.4 host release gate. Does not flash/monitor a physical board."""
from pathlib import Path
import subprocess,sys,json,shutil
r=Path(__file__).resolve().parents[1]
out=r/'build_reports/v244';out.mkdir(parents=True,exist_ok=True)
cases=[]
for name,command in [
 ('installer_stack_and_esp32_compile',[sys.executable,'tools/test_v244_install_reset.py']),
 ('existing_v243_regression',[sys.executable,'tools/verify_v243.py']),
 ('board_config',[sys.executable,'tools/check_board_config.py']),
]:
 try:
  proc=subprocess.run(command,cwd=r,text=True,capture_output=True,timeout=210)
  status='PASS' if proc.returncode==0 else 'FAIL'
  log=(proc.stdout or '')+'\n'+(proc.stderr or '')
  (out/f'{name}.log').write_text(log,encoding='utf-8')
  cases.append(dict(name=name,status=status,exit=proc.returncode))
  print(status,name,flush=True)
  if status=='FAIL':print(log[-2200:],flush=True)
 except Exception as e:
  cases.append(dict(name=name,status='FAIL',error=str(e)))
  print('FAIL',name,str(e),flush=True)
report={'release':'2.4.4 candidate','environment':'host (not physical ESP32-S3)',
  'cases':cases,'platformio_available':bool(shutil.which('pio')),
  'target_firmware_build':'NOT RUN','board_flash':'NOT RUN','reboot_on_device':'UNVERIFIED'}
(out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
lines=['# VQEAF OS v2.4.4 — Host gate','', '| Case | Status |','|---|---|']
lines += [f"| {i['name']} | {i['status']} |" for i in cases]
lines += ['','**No physical board, no PlatformIO build or hardware reset diagnosis in this gate.**']
(out/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Report:',out/'report.md')
raise SystemExit(int(any(c['status']!='PASS' for c in cases)))
