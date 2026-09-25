#!/usr/bin/env python3
"""Capture/parse VQEAF OS v2.5.1 ESP32-S3 115200 Serial performance.

Use --port COM5 --seconds 90 and manually navigate, or --input prior log
for fully reproducible offline parsing. Does not simulate hardware metrics.
"""
import argparse
import csv
import datetime as dt
import json
import re
import statistics
import sys
import time
from pathlib import Path

RENDER_PREFIX = "[VQEAF][FPS]"
LOOP_PREFIX = "[VQEAF][PERF]"
KEYVAL = re.compile(r"([A-Za-z][A-Za-z_0-9]*)=(-?[0-9]+)")
BOOT = re.compile(r"(?:\brst:0x|ESP-ROM:|\[QEAPP\]\[PREVIOUS_RESET\]|Guru Meditation|Brownout detector|Task watchdog|\[S3DIAG\]\[BOOT\])", re.I)
LAUNCH_FAIL = "[VQEAF][QEAPP][LAUNCH_FAIL]"


def parse_lines(rows, *, origin):
    """Rows are (ISO timestamp, decoded Serial line) preserving original data."""
    measurements, events, loops = [], [], []
    boot_events, launch_failures = 0, 0
    for timestamp, line in rows:
        if RENDER_PREFIX in line:
            fields = {k: int(v) for k, v in KEYVAL.findall(line.split(RENDER_PREFIX, 1)[1])}
            if "game_frames" in fields and "window_ms" in fields:
                measurements.append({"timestamp": timestamp, **fields})
        if LOOP_PREFIX in line:
            fields = {k: int(v) for k, v in KEYVAL.findall(line.split(LOOP_PREFIX, 1)[1])}
            if "samples" in fields:
                loops.append({"timestamp": timestamp, **fields})
        if BOOT.search(line):
            boot_events += 1
            events.append({"timestamp": timestamp, "event": "BOOT_OR_RESET_INDICATOR", "line": line})
        if LAUNCH_FAIL in line:
            launch_failures += 1
            events.append({"timestamp": timestamp, "event": "APP_LAUNCH_FAILURE", "line": line})
        if line.startswith("#HOST INSTALL_BEGIN") or line.startswith("#HOST BENCHMARK_START"):
            events.append({"timestamp": timestamp, "event": "HOST_MARKER", "line": line})
    total_ms = sum(r["window_ms"] for r in measurements)
    frames = sum(r["game_frames"] for r in measurements)
    def weighted(key, wt):
        data = [(r[key], r[wt]) for r in measurements if key in r and wt in r]
        mass = sum(w for _, w in data)
        return round(sum(value * w for value, w in data) / mass, 1) if mass else None
    result = {
        "origin": origin,
        "windows": len(measurements),
        "device_sampled_duration_s": round(total_ms / 1000, 3),
        "game_rendered_frames": frames,
        "game_effective_fps": round(frames * 1000. / total_ms, 2) if total_ms else None,
        "game_draw_avg_us_weighted": weighted("game_draw_avg_us", "game_frames"),
        "game_draw_max_us": max((r["game_draw_max_us"] for r in measurements if "game_draw_max_us" in r), default=None),
        "input_dispatch_avg_us_weighted": weighted("input_dispatch_avg_us", "input_events"),
        "input_dispatch_max_us": max((r["input_dispatch_max_us"] for r in measurements if "input_dispatch_max_us" in r), default=None),
        "boot_or_reset_indicator_lines": boot_events,
        "launch_failure_lines": launch_failures,
        "heap8_min_bytes": min((r["heap8"] for r in loops if "heap8" in r), default=None),
        "largest_internal_block_min_bytes": min((r["largest8"] for r in loops if "largest8" in r), default=None),
        "psram_free_min_bytes": min((r["psram"] for r in loops if "psram" in r), default=None),
        "notes": [
            "FPS counts actual Pixel Snake draw operations, not ST7789 refresh rate or idle UI FPS.",
            "Input event latency starts after input.poll() returns, ends after firmware dispatch; not GPIO edge-to-photon latency.",
            "USB auto-reset at Serial-open is NOT proof that QEAPP installation caused a reset.",
            "A non-hardware sample or mock log must never be represented as a real device benchmark.",
        ]
    }
    return result, measurements, loops, events


def capture(port, baud, seconds, interactive):
    try:
        import serial
    except ImportError as exc:
        raise SystemExit("pyserial unavailable. Run: py -3 -m pip install pyserial") from exc
    import threading
    stop = threading.Event()
    host_markers = []
    def keyboard():
        print("[Serial] Type i + Enter BEFORE installation, b + Enter before FPS test, q to stop.")
        while not stop.is_set():
            try: command = input().strip().lower()
            except (EOFError, KeyboardInterrupt): break
            if command in ("q", "quit"):
                stop.set()
                break
            if command in ("i", "b"):
                marker = "INSTALL_BEGIN" if command == "i" else "BENCHMARK_START"
                host_markers.append((dt.datetime.now(dt.timezone.utc).isoformat(), "#HOST " + marker))
                print("MARKER:", marker)
    if interactive:
        threading.Thread(target=keyboard, daemon=True).start()
    allrows, raw = [], bytearray()
    print(f"[Serial] Capture {port} at {baud} baud, {seconds}s max. Opening USB may reset board.")
    try:
        with serial.Serial(port, baudrate=baud, timeout=.4) as connection:
            started = time.monotonic()
            while not stop.is_set() and time.monotonic() - started < seconds:
                data = connection.read_until(b"\n")
                if not data: continue
                raw.extend(data)
                iso = dt.datetime.now(dt.timezone.utc).isoformat()
                line = data.decode("utf-8", errors="replace").rstrip("\r\n")
                allrows.append((iso, line))
                if BOOT.search(line) or RENDER_PREFIX in line or LAUNCH_FAIL in line:
                    print(iso, line[:220], flush=True)
    finally:
        stop.set()
    return sorted(allrows + host_markers, key=lambda r:r[0]), raw


def write_reports(directory, rows, raw, origin):
    directory.mkdir(parents=True, exist_ok=True)
    if raw is not None: (directory / "serial_raw.bin").write_bytes(raw)
    (directory / "serial_timestamped.log").write_text("\n".join(f"{t} {s}" for t,s in rows)+"\n",encoding="utf-8")
    result, measurements, loops, events = parse_lines(rows, origin=origin)
    result["note"] = "No hardware FPS conclusion possible" if origin != "serial_device" else "Derived from actual device Serial (verify port/device identity)"
    (directory / "metrics.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    for filename, records in (("fps_windows.csv", measurements),("loop_windows.csv", loops),("events.csv", events)):
        with (directory / filename).open("w",newline="",encoding="utf-8") as f:
            headers = sorted({k for row in records for k in row})
            writer = csv.DictWriter(f,fieldnames=headers)
            if headers: writer.writeheader();writer.writerows(records)
    def fmt(value,unit=""):
        return "Not measured" if value is None else f"{value}{unit}"
    md = ["# VQEAF OS v2.5.1 — Device serial analysis", "",
          f"Source: `{origin}`; captured windows: {result['windows']}; sample span: {result['device_sampled_duration_s']} s", "",
          "| Metric | Value |", "|---|---:|",
          f"| Game content-update FPS | {fmt(result['game_effective_fps'])} |",
          f"| Pixel Snake draw average | {fmt(result['game_draw_avg_us_weighted'],' us')} |",
          f"| Pixel Snake max draw | {fmt(result['game_draw_max_us'],' us')} |",
          f"| Input event-to-dispatch-completion average | {fmt(result['input_dispatch_avg_us_weighted'],' us')} |",
          f"| Input event-to-dispatch-completion max | {fmt(result['input_dispatch_max_us'],' us')} |",
          f"| Boot/reset indicator lines (not distinct resets) | {result['boot_or_reset_indicator_lines']} |",
          f"| App launch-failure lines | {result['launch_failure_lines']} |",
          f"| Minimum heap8 reported | {fmt(result['heap8_min_bytes'],' bytes')} |",
          f"| Minimum largest8 reported | {fmt(result['largest_internal_block_min_bytes'],' bytes')} |",
          f"| Minimum free PSRAM reported | {fmt(result['psram_free_min_bytes'],' bytes')} |", "",
          "## Interpretation and limitations", ""]
    md += ["- " + note for note in result["notes"]]
    (directory / "report.md").write_text("\n".join(md)+"\n",encoding="utf-8")
    return result


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    src=p.add_mutually_exclusive_group(required=True)
    src.add_argument("--port", help="Serial port on the actual ESP32-S3 (e.g. COM5)")
    src.add_argument("--input",type=Path,help="Existing plain Serial log for offline parsing")
    p.add_argument("--baud",type=int,default=115200)
    p.add_argument("--seconds",type=int,default=90)
    p.add_argument("--interactive",action="store_true",help="Enable i/b/q hotkeys from stdin")
    p.add_argument("--out",type=Path,default=Path("build_reports/device/v251"))
    a=p.parse_args(argv)
    if a.input:
        lines=a.input.read_text(encoding="utf-8",errors="replace").splitlines()
        # Accept both raw ESP32 lines and v2.4.4 logger timestamp prefix.
        stamp=dt.datetime.now(dt.timezone.utc).isoformat()
        stamped=[]
        for row in lines:
            match=re.match(r"^(\d{4}-\d\d-\d\dT\S+) (.*)$",row)
            stamped.append((match.group(1),match.group(2)) if match else (stamp,row))
        result=write_reports(a.out,stamped,None,"imported_log_not_proven_hardware")
    else:
        if a.baud!=115200:print("WARNING: firmware Serial is configured for 115200 baud",file=sys.stderr)
        rows,raw=capture(a.port,a.baud,a.seconds,a.interactive)
        result=write_reports(a.out,rows,raw,"serial_device")
    print(json.dumps({"report":str(a.out/"report.md"),"origin":result["origin"],
                      "fps":result["game_effective_fps"],"resets_indicated":result["boot_or_reset_indicator_lines"]},indent=2))
    return 0
if __name__ == "__main__":raise SystemExit(main())
