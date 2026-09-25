#!/usr/bin/env python3
"""Generate immutable 192-case on-device CRC fixtures from v234 *C++ renderer*.
Regenerate deliberately, review diff before updating reference; never derive
checksums from optimized source under test.
"""
import json
import subprocess
import tempfile
from pathlib import Path
R = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='vqeaf-golden-') as d:
    exe = Path(d)/'golden'
    subprocess.run(['g++','-std=c++11','-O2','-Wall','-Wextra','-Werror',
        '-I'+str(R/'docs/verification/v234_baseline'),
        '-I'+str(R/'tools/icon_host'),
        str(R/'docs/verification/v234_baseline/VqeafIconRenderer.cpp'),
        str(R/'tools/icon_device_host/generate_golden.cpp'),
        '-o',str(exe)],check=True)
    raw=subprocess.check_output([str(exe)],text=True)
    values=json.loads(raw)
assert len(values)==24 and all(len(v)==4 and all(len(p)==2 for p in v) for v in values)
header=['// GOLDEN: generated ONLY from frozen v2.3.4 C++ renderer, not v2.3.5.',
        '// 24 variants (id*2 + [36]) x 4 backgrounds x 2 clear modes.',
        '// CRC32 standard polynomial over LITTLE-ENDIAN row-major RGB565 tile.',
        '#pragma once','#include <stdint.h>','namespace VqeafIconGolden {',
        'static const uint16_t BACKGROUNDS[4] = {0x0000,0xFFFF,0x7BEF,0xF81F};',
        'static const uint32_t CRC[24][4][2] = {']
for i,entry in enumerate(values):
    name=('WiFi','Bluetooth','Music','Files','Gallery','Internet','Shell','Recovery','Settings','Themes','Apps','Library')[i//2]
    header.append('  { '+', '.join('{ '+', '.join(f'0x{c:08X}u' for c in cpair)+' }' for cpair in entry)+f' }}, // {name} '+('24' if i%2==0 else '36'))
header+=['};','} // namespace VqeafIconGolden','']
p=R/'docs/verification/device_icons/golden_crc_v234.h'
p.write_text('\n'.join(header),encoding='utf-8')
print('PASS golden from frozen baseline C++ renderer: 24 x 4 x 2 = 192 checksums at',p)
