#!/usr/bin/env python3
import importlib.util
import tempfile
from pathlib import Path
HERE = Path(__file__).resolve().parent
sp = importlib.util.spec_from_file_location('capture', HERE/'measure_v251_serial.py')
capture = importlib.util.module_from_spec(sp);sp.loader.exec_module(capture)
records=[
 ('2026-09-25T01:00:00+00:00','#HOST BENCHMARK_START'),
 ('2026-09-25T01:00:05+00:00','[VQEAF][FPS] window_ms=5000 game_frames=25 game_fps_x10=50 game_draw_avg_us=2100 game_draw_max_us=3100 game_draw_p95_le_us=4000 nav_count=2 nav_avg_us=12000 nav_max_us=14000 nav_p95_le_us=16000 input_events=12 input_dispatch_avg_us=5100 input_dispatch_max_us=9000 input_dispatch_p95_le_us=16000'),
 ('2026-09-25T01:00:10+00:00','[VQEAF][FPS] window_ms=5000 game_frames=35 game_fps_x10=70 game_draw_avg_us=2300 game_draw_max_us=4900 game_draw_p95_le_us=8000 nav_count=0 nav_avg_us=0 nav_max_us=0 nav_p95_le_us=0 input_events=8 input_dispatch_avg_us=6100 input_dispatch_max_us=12000 input_dispatch_p95_le_us=16000'),
 ('2026-09-25T01:00:10+00:00','[VQEAF][PERF] samples=580 avg_loop_us=7000 max_loop_us=13000 over16=0 over33=0 over100=0 heap8=215040 largest8=85952 psram=7000000'),
 ('2026-09-25T01:00:11+00:00','#HOST INSTALL_BEGIN'),
 ('2026-09-25T01:00:12+00:00','[VQEAF][QEAPP][LAUNCH_FAIL] id=wrong reason=receipt missing'),
 ('2026-09-25T01:00:13+00:00','rst:0x8 (TG1WDT_SYS_RESET)')]
with tempfile.TemporaryDirectory() as td:
    out=Path(td)
    result=capture.write_reports(out,records,b'fake serial','synthetic_unit_test')
    assert result['windows']==2 and result['game_rendered_frames']==60
    assert result['game_effective_fps']==6.0
    assert result['game_draw_avg_us_weighted']==2216.7
    assert result['input_dispatch_avg_us_weighted']==5500.0
    assert result['boot_or_reset_indicator_lines']==1 and result['launch_failure_lines']==1
    assert len((out/'fps_windows.csv').read_text().splitlines())==3
    assert (out/'report.md').exists() and (out/'serial_raw.bin').exists()
print('PASS: 2 synthetic windows, weighted FPS/draw/input, reset and launch-failure markers; NOT device readings')
