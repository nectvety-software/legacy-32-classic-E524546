#!/usr/bin/env python3
"""Compile two independent production C++ renderers; compare ALL rendered bytes.

Compares the frozen, unmodified v2.3.4 firmware source against the optimized
v2.3.5 firmware source across 24 sprites x 4 backgrounds x 2 clear modes.
Uncompressed output must match *exactly* (not visual tolerance).
"""
from pathlib import Path
import subprocess,sys,hashlib,tempfile,json
R=Path(__file__).resolve().parents[1]
REF=R/'docs/verification/v234_baseline'
SRC=R/'src/core'
C=R/'tools/icon_host/dump_all_pixels.cpp'
def run(cmd):
    p=subprocess.run(cmd,cwd=R,text=True,capture_output=True)
    if p.returncode:
        print(p.stdout,p.stderr,file=sys.stderr)
        raise RuntimeError(f'Failure ({p.returncode}): {cmd}')
    if p.stdout.strip():print(p.stdout.strip())
with tempfile.TemporaryDirectory(prefix='vqeaf-lossless-') as temp:
    d=Path(temp)
    files=[]
    for label,src in [('v234_baseline',REF),('v235_optimized',SRC)]:
        exe=d/label
        raw=d/(label+'.rgb565')
        run(['g++','-std=c++11','-O2','-Wall','-Wextra','-Werror',
             '-I'+str(src),'-I'+str(R/'tools/icon_host'),
             str(src/'VqeafIconRenderer.cpp'),str(C),'-o',str(exe)])
        run([str(exe),str(raw)])
        files.append(raw)
    data0,data1=[f.read_bytes() for f in files]
    expected=12*((24*24)+(36*36))*4*2*2
    if len(data0)!=expected or data0!=data1:
        first=next((i for i,(a,b) in enumerate(zip(data0,data1)) if a!=b),None)
        raise AssertionError(f'Framebuffer differs! first byte={first}; '
                             f'baseline={len(data0)} optimized={len(data1)}')
    print(f'PASS bit-perfect identical v2.3.4 / v2.3.5 RGB565: {expected} bytes '
          f'(24 icons x 4 bg x 2 clear modes, 179712 pixels), '
          f'SHA256={hashlib.sha256(data1).hexdigest()}')
    metrics=json.loads((R/'tools/pixel_icon_assets/icon_build_stats.json').read_text())
    assert metrics['baseline_payload_bytes']==8260
    assert metrics['optimized_payload_bytes']<metrics['baseline_payload_bytes']
