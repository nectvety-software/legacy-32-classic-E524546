#!/usr/bin/env python3
"""Verify lossless v2.3.4->v2.3.5 icon update and write a reproducible report.

Host checks do NOT imply an ESP32-S3 PlatformIO target build. A target build
must still be done with a cached PlatformIO/Arduino toolchain.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'build_reports' / 'icons_v235'
DEST.mkdir(parents=True, exist_ok=True)

COMMANDS = (
    ('generate', [sys.executable, 'tools/rebuild_vqeaf_icons.py']),
    ('parity_cpp', [sys.executable, 'tools/test_lossless_icons_v235.py']),
    ('parity_source_png', [sys.executable, 'tools/test_pixel_icons_v234.py']),
    ('home_menu_24_36_cpp', [sys.executable, 'tools/test_board_host_smoke.py']),
    ('board_configuration', [sys.executable, 'tools/check_board_config.py']),
)
results = []
for name, command in COMMANDS:
    start = time.monotonic()
    try:
        proc = subprocess.run(command, cwd=ROOT, capture_output=True,
                              text=True, timeout=90, check=False)
        success = proc.returncode == 0
        output = proc.stdout + ('\n[stderr]\n' + proc.stderr if proc.stderr else '')
        code = proc.returncode
    except subprocess.TimeoutExpired as e:
        success = False
        code = 'TIMEOUT'
        output = f'TIMEOUT: {e}\n'
    elapsed = round(time.monotonic() - start, 2)
    (DEST / (name+'.log')).write_text(output, encoding='utf-8')
    results.append(dict(step=name, passed=success, exit_code=code, seconds=elapsed))
    print(f'{"PASS" if success else "FAIL"}: {name} ({elapsed}s)')
    if not success:
        break

stats = json.loads((ROOT/'tools/pixel_icon_assets/icon_build_stats.json').read_text())
raw = (ROOT/'src/core/VqeafIconData.h').read_bytes()
orig = (ROOT/'docs/verification/v234_baseline/VqeafIconData.h').read_bytes()
# These measure the literal generated sprite byte arrays + local RGB565 palettes,
# not the entire final firmware, which is subject to architecture/linker effects.
assert stats['asset_count'] == 24
assert stats['baseline_payload_bytes'] == 8260
assert stats['optimized_payload_bytes'] == 6443
assert stats['saved_payload_bytes'] == 1817
assert stats['descriptor_size_esp32_before_and_after'] == 16
report = {
    'subject': 'VQEAF OS v2.3.5 RGB565 pixel-art icon optimization',
    'scope': '24 losslessly reconstructed sprites (12 names x 24/36), firmware icons only',
    'note': 'Host tests; does not demonstrate ESP32-S3 firmware build or hardware speed.',
    'source_version': 'v2.3.4', 'output_version': 'v2.3.5',
    'baseline_payload_bytes': stats['baseline_payload_bytes'],
    'optimized_payload_bytes': stats['optimized_payload_bytes'],
    'saved_bytes': stats['saved_payload_bytes'],
    'saved_percent': stats['saved_percent'],
    'header_source_text_bytes': {'baseline': len(orig), 'optimized': len(raw)},
    'header_sha256': hashlib.sha256(raw).hexdigest(),
    'parity_scenarios': 24*4*2,
    'parity_rgb565_pixels': 12*(24*24+36*36)*4*2,
    'checks': results,
    'passed': len(results) == len(COMMANDS) and all(r['passed'] for r in results),
    'esp32_platformio_build': 'not run',
    'physical_board_test': 'not run',
}
(DEST/'report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
lines = [
    '# VQEAF OS v2.3.5 — measured icon optimization',
    '', '| Metric | v2.3.4 | v2.3.5 |', '|---|---:|---:|',
    '| Sprite payload (bytes; excludes renderer/metadata) | 8,260 | 6,443 |',
    '| Saved asset bytes | — | 1,817 (22.00%) |',
    '| Variant sizes | 24×24, 36×36 | unchanged |',
    '| Icon count | 12 × 2 | unchanged |',
    f'| Source header size (not flash) | {len(orig):,} | {len(raw):,} |',
    '', '## Verification',
    'The old production renderer and optimized production renderer are compiled independently against the same TFT RGB565 host framebuffer.',
    f'Exhaustive expected coverage: **{report["parity_rgb565_pixels"]:,} pixel comparisons** across 192 scenarios (24 variants × 4 backgrounds × 2 clear modes).',
    '', '| Test | Result |', '|---|---|',
]
lines += [f'| {r["step"]} | {"PASS" if r["passed"] else "FAIL"} |' for r in results]
lines += ['', f'**Overall:** {"PASS" if report["passed"] else "FAIL"}', '',
          '**Not measured:** actual Xtensa firmware.bin Flash usage, on-device latency, SPI timing.',
          'Do not interpret a host PASS as a target firmware build.',
          '', '### Re-run', '```powershell', 'py -3 tools\\verify_icon_optimization_v235.py',
          'pio run -e vqeaf_os  # separate target test on a configured ESP32-S3 development PC', '```', '']
(DEST/'report.md').write_text('\n'.join(lines), encoding='utf-8')
print('Report:', DEST/'report.md')
if not report['passed']:
    sys.exit(1)
