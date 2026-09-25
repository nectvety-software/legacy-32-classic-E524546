#!/usr/bin/env python3
"""VQEAF ESP32-S3 lossless-on-receive UART capture and reboot timeline.

Records *every received byte* to raw_uart.bin before decoding or parsing. The
host cannot recover serial output emitted before the monitor opened or while
USB/COM is disconnected. Root cause is never inferred solely from a reconnect.

Requires pyserial only for physical capture; --demo and unit tests need stdlib.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import queue
import re
import sys
import threading
import time
from datetime import datetime, timezone
from typing import Callable

BAUD = 115200
MAX_LINE = 16384
STAGE_RE = re.compile(r"\[QEAPP\]\[INSTALL\]\[(\d{1,3})\]", re.I)
ESP_ROM_RE = re.compile(r"(?:^|\s)ESP-ROM:esp32s3", re.I)
RST_RE = re.compile(r"^\s*rst:0x[0-9a-f]+\s*\(", re.I)
DIAGNOSTICS = (
    (re.compile(r"Brownout detector was triggered|BROWNOUT", re.I), "BROWNOUT_TEXT"),
    (re.compile(r"stack canary watchpoint|Stack smashing|stack overflow", re.I), "STACK_FAILURE_TEXT"),
    (re.compile(r"Task watchdog got triggered|TASK_WDT", re.I), "TASK_WATCHDOG_TEXT"),
    (re.compile(r"Interrupt wdt timeout|INT_WDT", re.I), "INTERRUPT_WATCHDOG_TEXT"),
    (re.compile(r"Guru Meditation Error|assert failed|abort\(\)", re.I), "PANIC_OR_ABORT_TEXT"),
    (re.compile(r"\bBacktrace:\b|^Backtrace:", re.I), "BACKTRACE_TEXT"),
    (re.compile(r"\[QEAPP\]\[PREVIOUS_RESET\]", re.I), "PREVIOUS_RESET_MARKER"),
)


def stamp() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="milliseconds")


def clean_line(b: bytes) -> str:
    return b.rstrip(b"\r").decode("utf-8", "replace").replace("\x00", "\\x00")


class Recorder:
    """One writer for raw bytes + timeline. Other threads use command Queue."""

    def __init__(self, folder: Path, console=True, clock: Callable[[], float] = time.monotonic,
                 wall: Callable[[], str] = stamp, logfile_name: str = "serial_timestamped.log"):
        self.folder = Path(folder)
        self.folder.mkdir(parents=True, exist_ok=False)
        self.raw = (self.folder / "raw_uart.bin").open("wb")
        self.lines = (self.folder / logfile_name).open("w", encoding="utf-8", newline="\n")
        self.events_file = (self.folder / "events.jsonl").open("w", encoding="utf-8", newline="\n")
        self.console = console
        self.clock, self.wall = clock, wall
        self.start = self.clock()
        self.pending = bytearray()
        self.stats = {"received_bytes": 0, "serial_lines": 0, "serial_connections": 0,
                      "port_disconnects": 0, "boot_signatures": 0, "suspected_install_reboots": 0}
        self.events = []
        self.event_totals = {}
        self.install_active = False
        self.last_phase = None
        self.last_phase_at = None
        self.await_rst_until = -1e9
        self.boot_evidence_this_connection = False
        self.last_boot_at = None
        self.manual_reset_at = None
        self.saw_any_output = False
        self.stop_reason = "NORMAL_STOP"
        self.closed = False
        self.event("START", "Capture started; timestamps come from host clock")

    def event(self, kind: str, detail: str = "", **extra):
        at = self.wall()
        evt = {"wall_time": at, "seconds": round(self.clock() - self.start, 3),
               "kind": kind, "detail": detail, **extra}
        self.events.append(evt)
        self.event_totals[kind] = self.event_totals.get(kind, 0) + 1
        self.events_file.write(json.dumps(evt, ensure_ascii=False) + "\n")
        self.events_file.flush()
        self.lines.write(f"[{at}] [HOST_EVENT:{kind}] {detail}\n")
        self.lines.flush()
        if self.console:
            print(f"\n*** {at} {kind}: {detail}", flush=True)
        return evt

    def mark(self, label: str):
        label = label.strip()[:160] or "MANUAL_MARK"
        if label.lower().startswith("install"):
            self.boot_evidence_this_connection = False
            self.install_active = True
            self.last_phase = None
            self.event("USER_INSTALL_START", label)
        elif label.lower().startswith("reset"):
            self.manual_reset_at = self.clock()
            self.event("USER_RESET", label)
        elif label.lower().startswith("theme"):
            self.event("USER_THEME_START", label)
        else:
            self.event("USER_MARK", label)

    def connected(self, port: str, reconnect=False):
        self.boot_evidence_this_connection = False
        self.stats["serial_connections"] += 1
        self.event("PORT_RECONNECTED" if reconnect else "PORT_CONNECTED", f"Port {port}; UART 115200; host connection is not proof of device boot")

    def disconnected(self, error: str):
        # A truncated panic line must not be glued to the ROM header of a
        # different USB connection (the raw bytes remain unchanged).
        self.flush_pending()
        self.stats["port_disconnects"] += 1
        self.event("PORT_DISCONNECTED", str(error)[:350])

    def receive(self, chunk: bytes):
        if not chunk:
            return
        # Preserve received bytes *before* decoding or interpreting them.
        self.raw.write(chunk)
        self.raw.flush()
        self.stats["received_bytes"] += len(chunk)
        self.pending.extend(chunk)
        while True:
            try:
                ix = self.pending.index(10)
            except ValueError:
                if len(self.pending) > MAX_LINE:
                    seg = bytes(self.pending[:MAX_LINE])
                    del self.pending[:MAX_LINE]
                    self._line(seg, partial=True)
                break
            if ix > MAX_LINE:
                seg = bytes(self.pending[:MAX_LINE])
                del self.pending[:MAX_LINE]
                self._line(seg, partial=True)
                continue
            b = bytes(self.pending[:ix])
            del self.pending[:ix + 1]
            self._line(b)

    def _line(self, data: bytes, partial=False):
        line = clean_line(data)
        at = self.wall()
        self.stats["serial_lines"] += 1
        self.lines.write(f"[{at}] [+{self.clock()-self.start:.3f}s] {line}" + (" [PARTIAL_SEGMENT]" if partial else "") + "\n")
        self.lines.flush()
        if self.console:
            print(f"{at} {line}" + (" [PARTIAL_SEGMENT]" if partial else ""), flush=True)
        if not partial:
            self._classify(line)
        self.saw_any_output = True

    def _classify(self, line: str):
        now = self.clock()
        m = STAGE_RE.search(line)
        if m:
            phase = int(m.group(1))
            self.last_phase = phase
            self.last_phase_at = self.wall()
            if phase <= 10:
                self.boot_evidence_this_connection = False
            if phase < 100:
                self.install_active = True
            self.event("INSTALL_STAGE", f"Stage {phase}: {line[:220]}", stage=phase)
            if phase >= 100 or re.search(r"install_complete\b", line, re.I):
                self.install_active = False
                self.event("INSTALL_COMPLETE", "Device emitted installation completion marker")
        for pattern, kind in DIAGNOSTICS:
            if pattern.search(line):
                self.event(kind, line[:300])
        # On native USB CDC the ROM UART header may never reach this port;
        # the firmware's RTC previous-reset banner is weaker but useful
        # fallback boot evidence if no ROM signature was received on this connection.
        if "[QEAPP][PREVIOUS_RESET]" in line.upper() and not self.boot_evidence_this_connection:
            self._record_boot(line, source="FIRMWARE_PREVIOUS_RESET_MARKER")
        # ESP32-S3 usually emits ESP-ROM then rst:. Count every ESP-ROM,
        # including a rapid boot loop (<2 s between ROM headers). Suppress
        # only a matching rst: line shortly after that same ROM header.
        is_rom = bool(ESP_ROM_RE.search(line))
        is_rst = bool(RST_RE.search(line))
        if is_rom or (is_rst and now > self.await_rst_until):
            self.await_rst_until = now + 1.2 if is_rom else -1e9
            self._record_boot(line, source="ESP_ROM_UART")
        elif is_rst:
            self.await_rst_until = -1e9

    def _record_boot(self, line: str, source: str):
        now = self.clock()
        self.boot_evidence_this_connection = True
        self.last_boot_at = self.wall()
        self.stats["boot_signatures"] += 1
        is_manual = self.manual_reset_at is not None and now - self.manual_reset_at < 15
        in_install = self.install_active and not is_manual
        kind = "BOOT_DURING_INSTALL_SUSPECTED" if in_install else "BOOT_AFTER_MANUAL_RESET" if is_manual else "BOOT_OBSERVED"
        if in_install:
            self.stats["suspected_install_reboots"] += 1
        self.event(kind, line[:300], last_phase=self.last_phase,
                   last_phase_at=self.last_phase_at,
                   evidence=source + "; no automatic attribution of reset cause")
        self.manual_reset_at = None
        self.install_active = False



    def flush_pending(self):
        if self.pending:
            b = bytes(self.pending)
            self.pending.clear()
            self._line(b, partial=True)

    def close(self, reason="NORMAL_STOP"):
        if self.closed:
            return
        self.stop_reason = reason
        self.flush_pending()
        self.event("STOP", reason)
        self.raw.close(); self.lines.close(); self.events_file.close()
        result = {
            "tool": "VQEAF UART Capture v1", "baud": BAUD,
            "captured_bytes": self.stats["received_bytes"],
            "stats": self.stats, "event_totals": self.event_totals,
            "last_install_phase": self.last_phase, "last_phase_time": self.last_phase_at,
            "last_boot_time": self.last_boot_at,
            "end_reason": reason,
            "notes": [
                "A reconnect alone is not evidence of ESP32 reset.",
                "UART before port open / while disconnected cannot be recovered.",
                "A boot during install is suspected timing, not identified root cause.",
                "Share logs privately after checking for credentials or tokens.",
            ],
        }
        (self.folder / "summary.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        # Bound report length: complete event history remains in events.jsonl.
        significant = [x for x in self.events if x["kind"] not in ("INSTALL_STAGE",)]
        lines = ["# VQEAF ESP32-S3 — Báo cáo thu thập Serial 115200", "",
                 f"- Byte UART đã nhận: **{self.stats['received_bytes']:,}**",
                 f"- Dòng Serial: **{self.stats['serial_lines']:,}**",
                 f"- Dấu hiệu boot (ROM UART / firmware): **{self.stats['boot_signatures']}**",
                 f"- Nghi vấn boot trong khi cài: **{self.stats['suspected_install_reboots']}**",
                 f"- Giai đoạn cài đặt cuối: **{self.last_phase if self.last_phase is not None else 'không quan sát được'}**",
                 "", "## Timeline sự kiện nổi bật", ""]
        for x in significant[-200:]:
            lines.append(f"- {x['wall_time']} [{x['kind']}] {x['detail']}")
        if len(significant) > 200:
            lines.append(f"- … thêm {len(significant)-200} sự kiện ở events.jsonl")
        lines.extend(["", "## Lưu ý", "", "Dữ liệu gốc raw_uart.bin ghi lại đầy đủ mọi byte **đã nhận được**;", 
                      "Serial có thể mất dữ liệu khi cáp/ngõ USB bị ngắt hoặc lúc cổng chưa mở.",
                      "Nhãn BOOT_DURING_INSTALL_SUSPECTED không xác định nguyên nhân reset.",
                      "Đọc panic/backtrace nguyên văn trong serial_timestamped.log.",
                      "Không chia sẻ log công khai nếu có SSID, token, URL nhạy cảm.", ""])
        (self.folder / "report.md").write_text("\n".join(lines), encoding="utf-8")
        self.closed = True
        return result


def choose_port(serial_module, requested):
    if requested.upper() != "AUTO":
        return requested
    ports = list(serial_module.tools.list_ports.comports())
    if not ports:
        raise ValueError("Không tìm thấy cổng. Kết nối ESP32-S3, kiểm tra USB CDC/driver và --list-ports.")
    candidates = [x for x in ports if getattr(x, "vid", None) == 0x303A]
    if len(candidates) == 1:
        return candidates[0].device
    if len(ports) == 1:
        return ports[0].device
    raise ValueError("Nhiều cổng serial; chỉ định --port COMx (Windows) hoặc /dev/ttyACM0. Có: " + ", ".join(x.device for x in ports))


def open_uart(serial_module, port, baud, *, dtr=False, rts=False):
    # Default control-line states are deasserted. Some USB drivers can still
    # pulse/reset upon opening; document and mark initial boot accordingly.
    uart = serial_module.Serial(port=None, baudrate=baud, timeout=0.2, write_timeout=0.2)
    uart.dtr = bool(dtr)
    uart.rts = bool(rts)
    uart.port = port
    uart.open()
    return uart


def keyboard_thread(q: queue.Queue, stop: threading.Event):
    if not sys.stdin.isatty():
        return
    print("Nhấn ENTER để đánh dấu, hoặc i=cài app; t=theme; r=RESET thủ công; m <ghi chú>; q=thoát.", flush=True)
    while not stop.is_set():
        try:
            command = input().strip()
        except (EOFError, OSError):
            return
        if not command:
            q.put(("mark", "MANUAL_MARK"))
        elif command.lower() in ("q", "quit", "exit"):
            q.put(("quit", "")); return
        elif command.lower() in ("i", "install"):
            q.put(("mark", "install: user pressed Install"))
        elif command.lower() in ("t", "theme"):
            q.put(("mark", "theme: user pressed Apply"))
        elif command.lower() in ("r", "reset"):
            q.put(("mark", "reset: intentional board reset"))
        elif command.lower().startswith("m "):
            q.put(("mark", command[2:].strip()))
        else:
            q.put(("mark", command))


def run_capture(args, serial_module, clock=time.monotonic, sleep=time.sleep):
    root = Path(args.out)
    if args.output is not None:
        root = Path(args.output).parent
    root.mkdir(parents=True, exist_ok=True)
    session = datetime.now().astimezone().strftime("%Y%m%d_%H%M%S_%f")
    folder = root / f"vqeaf_uart_{session}"
    recorder = Recorder(folder, console=not args.quiet,
                        logfile_name="serial_timestamped.log")
    stop = threading.Event()
    commands = queue.Queue()
    if not args.no_keyboard:
        threading.Thread(target=keyboard_thread, args=(commands, stop), daemon=True).start()
    connected_once = False
    uart = None
    started = clock()
    next_attempt = 0.0
    status_at = started
    reason = "STOPPED"
    print(f"\nSAVE SESSION: {folder.resolve()}\n120-second replay or unlimited (0): {args.duration}s")
    try:
        while True:
            now = clock()
            if args.duration > 0 and now - started >= args.duration:
                reason = "DURATION_REACHED"; break
            while True:
                try:
                    command, value = commands.get_nowait()
                except queue.Empty:
                    break
                if command == "quit":
                    reason = "USER_QUIT"; return folder
                recorder.mark(value)
            if uart is None:
                if now < next_attempt:
                    sleep(0.05)
                    continue
                try:
                    port = choose_port(serial_module, args.port)
                    uart = open_uart(serial_module, port, args.baud, dtr=args.dtr, rts=args.rts)
                    recorder.connected(port, reconnect=connected_once)
                    connected_once = True
                    status_at = clock()
                except (OSError, ValueError, serial_module.SerialException) as exc:
                    # An unopened serial port (including initial connection) is not a device reset.
                    if not args.reconnect:
                        recorder.event("PORT_OPEN_ERROR", str(exc))
                        reason = "PORT_OPEN_FAILED"
                        break
                    if now >= next_attempt:
                        recorder.event("WAITING_FOR_PORT", str(exc)[:250])
                    next_attempt = now + max(0.4, args.retry_seconds)
                    sleep(0.1)
                    continue
            try:
                payload = uart.read(4096)
                if payload:
                    recorder.receive(payload)
                    status_at = clock()
                elif not args.quiet and clock() - status_at >= args.idle_warning:
                    print("[WAIT] Chưa có UART mới. Nếu cần chụp ROM boot, nhấn RESET SAU KHI mở công cụ.", flush=True)
                    status_at = clock()
            except (OSError, serial_module.SerialException) as exc:
                recorder.disconnected(str(exc))
                try:
                    uart.close()
                except Exception:
                    pass
                uart = None
                next_attempt = clock() + max(0.4, args.retry_seconds)
                if not args.reconnect:
                    reason = "PORT_READ_FAILED"; break
    except KeyboardInterrupt:
        reason = "USER_CTRL_C"
    finally:
        stop.set()
        if uart is not None:
            try: uart.close()
            except Exception: pass
        result = recorder.close(reason)
        if args.output is not None:
            # Preserve the old --output CLI as a convenience copy. All original
            # evidence, including raw UART, lives under the unique session.
            import shutil
            shutil.copyfile(folder / "serial_timestamped.log", args.output)
        print("\n=== CAPTURE COMPLETE ===")
        print("Session:", folder.resolve())
        print("Received:", result["captured_bytes"], "bytes")
        print("Boot signatures:", result["stats"]["boot_signatures"])
        print("Install boot suspects:", result["stats"]["suspected_install_reboots"])
        for name in ("raw_uart.bin", "serial_timestamped.log", "events.jsonl", "report.md", "summary.json"):
            print(" -", name)
    return folder


def make_demo(args):
    root = Path(args.out)
    root.mkdir(parents=True, exist_ok=True)
    folder = root / "DEMO_SIMULATED_DO_NOT_TREAT_AS_HARDWARE"
    ix = 1
    while folder.exists():
        folder = root / f"DEMO_SIMULATED_DO_NOT_TREAT_AS_HARDWARE_{ix}"
        ix += 1
    rec = Recorder(folder, console=True)
    rec.event("SIMULATION_ONLY", "ALL UART bytes below are synthetic demo data")
    rec.connected("SIMULATED")
    rec.receive(b"[QEAPP][INSTALL][10] install_entry heap=...\r\n")
    rec.receive(b"[QEAPP][INSTALL][50] copy_complete\r\n")
    rec.receive(b"Guru Meditation Error: Core 1 panic\r\nBacktrace: 0x40000000\r\n")
    rec.disconnected("SIMULATED: USB re-enumeration")
    rec.connected("SIMULATED", reconnect=True)
    rec.receive(b"ESP-ROM:esp32s3-20210327\r\nrst:0xc (SW_CPU_RESET),boot:0x8\r\n")
    rec.receive(b"[QEAPP][PREVIOUS_RESET] installer_phase=50\r\n")
    rec.close("DEMO_COMPLETE")
    print("DEMO only (NOT from device):", folder.resolve())


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    p.add_argument("--port", default="auto", help="COM5, /dev/ttyACM0 or auto")
    p.add_argument("--baud", type=int, default=BAUD, help="UART speed; firmware emits 115200")
    p.add_argument("--out", type=Path, default=Path("build_reports/device_serial"), help="Parent folder for unique recording sessions")
    p.add_argument("--output", type=Path, default=None, help="Legacy alias: use its parent as --out (actual log saved in unique session folder)")
    p.add_argument("--duration", type=float, default=0.0, help="Stop after N seconds; 0 means until Ctrl+C/q")
    p.add_argument("--reconnect", action=argparse.BooleanOptionalAction, default=True, help="Wait for USB COM to return after disconnect")
    p.add_argument("--retry-seconds", type=float, default=1.5)
    p.add_argument("--idle-warning", type=float, default=12)
    p.add_argument("--quiet", action="store_true", help="Do not print each UART line to terminal")
    p.add_argument("--dtr", action="store_true", help="Assert DTR for USB CDC requiring it; MAY affect board reset")
    p.add_argument("--rts", action="store_true", help="Assert RTS; not recommended on ESP32 auto-program circuits")
    p.add_argument("--no-keyboard", action="store_true", help="Disable manual marker commands (for unattended runs)")
    p.add_argument("--list-ports", action="store_true", help="Print available serial ports and exit")
    p.add_argument("--demo", action="store_true", help="Run a visibly SIMULATED offline demo; never claims physical UART capture")
    args = p.parse_args(argv)
    if args.baud <= 0 or args.retry_seconds <= 0 or args.idle_warning <= 0 or args.duration < 0:
        p.error("Numeric timing and baud values must be positive (duration may be 0)")
    if args.demo:
        make_demo(args); return 0
    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        p.error("pyserial missing. Install: py -3 -m pip install -r tools/serial_requirements.txt")
    if args.list_ports:
        for x in serial.tools.list_ports.comports():
            print(f"{x.device:22} {x.description:45} VID:PID={x.vid}:{x.pid}")
        return 0
    folder = run_capture(args, serial)
    # The caller decides if a suspected reset is a software defect based on UART and board tests.
    j = json.loads((folder / "summary.json").read_text(encoding="utf-8"))
    if j["stats"]["received_bytes"] == 0:
        print("WARNING: No UART captured. Verify COM port/USB CDC and press board RESET.")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
