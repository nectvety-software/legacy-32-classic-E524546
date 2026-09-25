#!/usr/bin/env python3
"""Test preprocessor target guards with host C++ compiler (not an MCU build)."""
import subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parent.parent
source='#include "core/BuildSanity.h"\nint main(){ return VqeafBuildSanity::AllUnique<1,2,3>::value ? 0 : 1; }\n'
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'guard.cpp').write_text(source)
 base=['g++','-std=c++11','-Werror','-I'+str(r/'tools/host_stubs'),
       '-I'+str(r/'include'),'-I'+str(r/'src'),'-DARDUINO_ARCH_ESP32','-c',str(p/'guard.cpp'),'-o',str(p/'guard.o')]
 for name,extra,expected in [('correct S3+PSRAM',['-DCONFIG_IDF_TARGET_ESP32S3','-DBOARD_HAS_PSRAM'],0),
                              ('wrong ESP target',['-DBOARD_HAS_PSRAM'],1),
                              ('missing PSRAM',['-DCONFIG_IDF_TARGET_ESP32S3'],1)]:
  result=subprocess.run([*base,*extra],text=True,capture_output=True)
  assert (result.returncode==0)==(expected==0),f'{name}: unexpected result: {result.stderr}'
  print(f'PASS build guard: {name} -> {"compile" if expected==0 else "expected compile error"}')
print('NOTE: only sanity header guarded; this is NOT a PlatformIO SDK compile')
