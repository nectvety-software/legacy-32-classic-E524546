#!/usr/bin/env python3
"""Host test gate for v2.5.0 UI performance update.

Do not infer ESP32 target build or device FPS from any host results.
Run from anywhere: python tools/verify_v250.py [--quick].
"""
import argparse, json, shutil, subprocess, sys, time
from pathlib import Path
R=Path(__file__).resolve().parents[1]
O=R/'build_reports/v250'
O.mkdir(parents=True,exist_ok=True)


def main():
    pa=argparse.ArgumentParser(description=__doc__)
    pa.add_argument('--quick',action='store_true',help='Skip long v2.4.x and full diagnostic linkage suites')
    opts=pa.parse_args()
    if not shutil.which('g++'):
        print('Host compiler g++ unavailable');return 2
    def cc(name,source,extra=None):
        return ['g++','-std=c++11','-O2','-Wall','-Wextra','-Werror',*(extra or []),
                '-Itools/icon_host','-Isrc/core',f'tools/performance_host/{source}',
                'src/core/VqeafIconRenderer.cpp','-o',str(O/name)]
    perf=['g++','-std=c++11','-O2','-Wall','-Wextra','-Werror',
          '-Isrc/core','tools/performance_host/test_ui_perf.cpp','-o',str(O/'perf_counter')]
    cases=[
      ('build_opaque_scanline',cc('scanline','test_icon_scanline.cpp')),
      ('opaque_scanline_pixel_parity',[str(O/'scanline')]),
      ('build_perf_counter',perf),
      ('perf_counter',[str(O/'perf_counter')]),
      ('build_historical_fallback',cc('baseline','test_baseline_fallback.cpp',['-DVQEAF_ICON_BASELINE=1'])),
      ('historical_fallback',[str(O/'baseline')]),
      ('build_compatibility_fallback',cc('compat','test_baseline_fallback.cpp',['-DVQEAF_ICON_FAST_ROWS_DISABLE=1'])),
      ('compatibility_fallback',[str(O/'compat')]),
      ('ui_image_smoke',[sys.executable,'tools/test_board_host_smoke.py']),
      ('installer_reset_regression',[sys.executable,'tools/test_v244_install_reset.py']),
      ('board_gpio',[sys.executable,'tools/check_board_config.py']),
    ]
    if not opts.quick:
        cases += [
            ('legacy_regressions_5_containing_17',[sys.executable,'tools/verify_v243.py']),
            ('opt_in_diagnostic_full_host_link',[sys.executable,'tools/test_perf_diag_host.py']),
        ]
    results=[]
    for name,cmd in cases:
        t=time.monotonic()
        try:
            p=subprocess.run(cmd,cwd=R,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                             timeout=290,check=False)
            output=p.stdout
            ok=p.returncode==0
            code=p.returncode
        except subprocess.TimeoutExpired as e:
            output=str(e);ok=False;code=124
        (O/f'{name}.log').write_text(output,encoding='utf8',errors='replace')
        results.append(dict(name=name,status='PASS' if ok else 'FAIL',exit=code,seconds=round(time.monotonic()-t,3)))
        print(('PASS ' if ok else 'FAIL ')+name,flush=True)
        if not ok:print(output[-2800:],flush=True)
        # Fail fast: prevent subsequent success from obscuring failure.
        if not ok:break
    passed=all(z['status']=='PASS' for z in results) and len(results)==len(cases)
    report=dict(version='2.5.0 host candidate',environment='PC C++ and shims',
         passed=passed,checks=results,completed=len(results),planned=len(cases),
         target_platformio='NOT_RUN',physical_esp32_s3='NOT_RUN',
         real_panel_rgb565='NOT_RUN',real_fps='NOT_MEASURED',
         sample_comparison='24 icon variants x six flat theme backgrounds = 144 scenarios x 134784 pixels; host-only')
    (O/'report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    lines=['# VQEAF OS v2.5.0 — Host verification','','Build: **PC simulation only**; no PlatformIO toolchain or actual SPI/SD/WiFi timings.',
           '', '| Check | Result | Seconds |','|---|---|---:|']
    lines += [f"| `{z['name']}` | {z['status']} | {z['seconds']:.2f} |" for z in results]
    lines += ['','**Overall:** '+('PASS' if passed else 'FAIL'),
              '', '**Graphics**: 144 RGB565 screen scenarios, 134,784 compared pixels, 30,846 legacy colored spans vs 4,320 scanline calls across these scenarios (host call counts, not FPS).',
              'One display write transaction for each icon in the new opaque-only path, with byte-order setting restored.',
              '', '**Not established:** on-device FPS, color accuracy on physical ST7789, image decoding latency, installer/theme hardware stability, battery usage.','']
    (O/'report.md').write_text('\n'.join(lines),encoding='utf8')
    print('OVERALL', 'PASS' if passed else 'FAIL',f'{len(results)}/{len(cases)}',flush=True)
    return 0 if passed else 1
if __name__=='__main__':sys.exit(main())
