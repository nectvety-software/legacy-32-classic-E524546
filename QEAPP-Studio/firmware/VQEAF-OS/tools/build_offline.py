#!/usr/bin/env python3
"""VQEAF OS v2.3.3 offline PlatformIO builder (Python 3.8+, stdlib only).

No pip install, pio pkg install, or pio upgrade. Uses an existing PlatformIO
cache, project-local board and preinstalled project library dependencies.

IMPORTANT: a rejected HTTP proxy is best-effort protection against accidental
PlatformIO requests, not an OS-level network sandbox. Disconnect networking
or use a system firewall/network namespace when strict isolation is required.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import textwrap
import traceback
from typing import Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parent.parent
ENV = "vqeaf_os"
PLATFORM = "espressif32"
PIN_PLATFORM = "6.10.0"
REQUIRED_PACKAGES = {
    "toolchain-xtensa-esp32s3": "ESP32-S3 cross-compiler",
    "framework-arduinoespressif32": "Arduino framework",
    "tool-esptoolpy": "ESP32 image/flash tool",
    "tool-scons": "PlatformIO build executor",
}
LIBRARIES = {
    "TFT_eSPI": ("TFT_eSPI", "2.5.43"),
    "NimBLE-Arduino": ("NimBLE-Arduino", "2.3.6"),
    "TJpg_Decoder": ("TJpg_Decoder", "1.1.0"),
    "PNGdec": ("PNGdec", "1.1.6"),
}
DRY_RUN_FAILURES = (
    "none", "board", "platformio", "toolchain", "library", "clean",
    "compiler", "linker", "network", "missing-bin", "buildfs",
)
ERROR_RULES = [
    ("MISSING_LIBRARY", r"fatal error:\s*([\w/.-]+\.h): No such file or directory|Could not find (?:the )?library", "Kiểm tra lib_deps và .pio/libdeps/vqeaf_os; nạp đúng phiên bản thư viện từ cache máy đã build online."),
    ("MISSING_PACKAGE", r"PackageNotFoundError|Could not find the package|toolchain-.*not found|framework-arduinoespressif32.*not found", "Sao chép gói PlatformIO còn thiếu vào PLATFORMIO_CORE_DIR/packages."),
    ("UNKNOWN_BOARD", r"UnknownBoard|Unknown board|Board.*not found", "Kiểm tra boards/vqeaf_s3_n16r8.json và board = vqeaf_s3_n16r8."),
    ("LINKER", r"undefined reference to|collect2(?:\.exe)?: error|ld(?:\.exe)?:.*(?:undefined|error)", "Kiểm tra các biểu tượng linker ở log và thứ tự thư viện; không xử lý bằng cách tắt lỗi."),
    ("FLASH_OVERFLOW", r"region `[^`]+` overflowed|(?:RAM|Flash).*overflow|Sketch too big|Program size is greater", "Kiểm tra linker map, OTA app slot và kích thước thư viện."),
    ("NETWORK_ACCESS", r"ProxyError|ConnectionError|Failed to establish a new connection|Name or service not known|ConnectTimeout|Temporary failure in name resolution", "Thiếu gói cache; offline build không tự tải. Sao chép packages/platforms/libdeps từ PC đã cài."),
    ("PERMISSION", r"Permission denied|Access is denied|WinError 5", "Dùng thư mục có quyền ghi và kiểm tra antivirus/đường dẫn Windows."),
    ("COMPILER", r"\berror:\s|\bfatal error:\s", "Xem dòng error đầu tiên trong build.log cùng 3 dòng trước đó; sửa source hoặc phiên bản thư viện tương ứng."),
]


def now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def package_version(path: Path) -> Optional[str]:
    j = path / "package.json"
    if j.is_file():
        try:
            return str(json.loads(j.read_text(encoding="utf-8"))["version"])
        except (ValueError, KeyError):
            return None
    j = path / "library.json"
    if j.is_file():
        try:
            return str(json.loads(j.read_text(encoding="utf-8"))["version"])
        except (ValueError, KeyError):
            return None
    p = path / "library.properties"
    if p.is_file():
        match = re.search(r"^version\s*=\s*([^\r\n]+)", p.read_text(encoding="utf-8", errors="replace"), re.M)
        if match:
            return match.group(1).strip()
    return None


def select_pio(explicit: Optional[str]) -> Optional[List[str]]:
    if explicit:
        p = Path(explicit).expanduser()
        if p.is_file():
            return [str(p)]
        if shutil.which(explicit):
            return [str(shutil.which(explicit))]
        return None
    p = shutil.which("pio") or shutil.which("platformio")
    if p:
        return [p]
    if importlib.util.find_spec("platformio"):
        return [sys.executable, "-m", "platformio"]
    return None


def diagnostic_lines(raw: str) -> List[dict]:
    result: List[dict] = []
    lines = raw.splitlines()
    for category, expression, suggestion in ERROR_RULES:
        pat = re.compile(expression, re.I)
        hits = [(i + 1, lines[i].strip()[:280]) for i in range(len(lines)) if pat.search(lines[i])]
        if category == "COMPILER":
            # collect2/ld reports are linker failures, NOT extra C++ syntax errors.
            hits = [hit for hit in hits if not re.search(
                r"collect2|undefined reference|(?:^|[/\\])ld(?:\.exe)?:", hit[1], re.I)]
        if category == "COMPILER":
            # A linker line such as "collect2.exe: error: ld returned 1 exit status"
            # is not evidence of a compiler syntax error. Keep separate causes
            # actionable instead of reporting a misleading duplicate.
            hits = [(line, msg) for line, msg in hits
                    if re.search(r"\bfatal error:|(?:\.(?:c|cc|cpp|h|hpp):\d+(?::\d+)?:\s*)error:", msg, re.I)]
            if not hits and not result and re.search(r"\berror:\s", raw, re.I):
                hits = [(i + 1, lines[i].strip()[:280]) for i in range(len(lines))
                        if re.search(r"\berror:\s", lines[i], re.I)]
        if hits:
            result.append({"code": category, "examples": [{"line": line, "text": msg} for line, msg in hits[:3]], "suggestion_vi": suggestion})
    if not result and raw.strip():
        result.append({"code": "UNKNOWN", "examples": [{"line": -1, "text": raw.splitlines()[-1][-280:]}], "suggestion_vi": "Xem build.log; gửi báo cáo và 40 dòng cuối log khi báo lỗi."})
    return result


class Build:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        # Keep simulations separate from real build reports and stale build artifacts.
        self.report_dir = (Path(args.report_dir).resolve() if args.report_dir else
                           ROOT / "build_reports" / "offline" / ("dry_run" if args.dry_run else ""))
        self.report_dir.mkdir(parents=True, exist_ok=True)
        if args.dry_run:
            # Avoid mixing old simulated output with the new run, including
            # stale build.log on --check-only and accumulated steps.log.
            for filename in ("preflight.log", "preflight_simulated.log", "board_preflight.log",
                             "steps.log", "platformio_version.log", "host_cpp.log",
                             "clean.log", "compile.log", "link.log", "build.log",
                             "artifact_check.log", "buildfs.log", "dry_run_plan.log",
                             "report.json", "report.md", "internal_error.log"):
                ((self.report_dir / filename).unlink() if (self.report_dir / filename).exists() else None)
        self.home = Path(args.pio_home).expanduser().resolve() if args.pio_home else Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio"))).expanduser().resolve()
        # A dry-run must never probe the CLI by executing it, nor launch a
        # cross-compiler; even --pio pointing to a fake executable is ignored.
        self.pio = None if args.dry_run else select_pio(args.pio)
        self.report: Dict = {
            "timestamp_utc": now(), "project": "VQEAF OS v2.3.3", "environment": ENV,
            "mode": "dry_run_simulation" if args.dry_run else "offline_preinstalled_cache", "pio_home": str(self.home),
            "dry_run": bool(args.dry_run), "simulated_failure": args.simulate_failure,
            "board_preflight": "NOT_RUN", "dependency_cache": "NOT_RUN", "platformio_version": None,
            "host_tests": "SKIPPED", "clean": "NOT_RUN", "target_build": "NOT_RUN",
            "buildfs": "NOT_REQUESTED", "firmware_bin": "NOT_GENERATED",
            "status": "STARTED", "missing": [], "warnings": [], "diagnostics": [], "artifacts": [],
            "commands": [], "network_guard": "rejecting_http_proxy_best_effort",
        }
        if args.dry_run:
            self.report.update({
                "execution_policy": "STATIC_BOARD_CHECK_ONLY; NO_PLATFORMIO_OR_TOOLCHAIN_EXECUTION",
                "network_guard": "not_applicable_no_platformio_process",
                "simulated_steps": [], "planned_commands": [],
                "observed_cache": {}, "would_generate": [],
            })
        self.messages: List[str] = []
        self.env = os.environ.copy()
        self.env["PLATFORMIO_CORE_DIR"] = str(self.home)
        self.env["PLATFORMIO_DISABLE_PROGRESSBAR"] = "1"
        self.env["PLATFORMIO_SETTING_ENABLE_TELEMETRY"] = "No"
        # Requests/urllib proxy guard: fails fast if PIO tries fetching online.
        # It is not a firewall: users requiring absolute offline isolation should
        # disconnect WiFi/ethernet or run in OS-enforced network sandbox.
        for key in ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy"):
            self.env[key] = "http://127.0.0.1:9"
        self.env["NO_PROXY"] = "localhost,127.0.0.1"
        self.env["no_proxy"] = "localhost,127.0.0.1"

    def dry_step(self, name: str, logfile: str, command: str,
                 result: str = "SIMULATED", detail: str = "") -> None:
        """Write a synthetic stage without launching PlatformIO or a toolchain."""
        record = {"step": name, "command": command, "result": result,
                  "executed": False, "detail": detail}
        self.report["simulated_steps"].append(record)
        self.report["planned_commands"].append(command)
        (self.report_dir / logfile).write_text(
            f"[DRY-RUN: NOT EXECUTED] {now()}\n"
            f"Stage: {name}\nWould run: {command}\nResult: {result}\n{detail}\n",
            encoding="utf-8",
        )
        self.log(f"[DRY-RUN] {name}: {result}", "steps.log")

    def dry_probe(self) -> None:
        """Read-only environment snapshot; observations never count as readiness."""
        observed = self.report["observed_cache"]
        observed["platformio_cli_detected"] = bool(
            shutil.which("pio") or shutil.which("platformio") or
            importlib.util.find_spec("platformio")
        )
        platform_json = self.home / "platforms" / PLATFORM / "platform.json"
        observed["platform"] = {"present": platform_json.is_file(),
                                "version": package_version(platform_json.parent) if platform_json.is_file() else None}
        # The platform metadata may be in platform.json, not package.json.
        if platform_json.is_file():
            try:
                observed["platform"]["version"] = json.loads(platform_json.read_text(encoding="utf-8")).get("version")
            except (ValueError, OSError) as exc:
                observed["platform"]["read_error"] = str(exc)
        observed["packages"] = {
            name: {"present": (self.home / "packages" / name / "package.json").is_file(),
                   "version": package_version(self.home / "packages" / name)}
            for name in REQUIRED_PACKAGES
        }
        base = ROOT / ".pio" / "libdeps" / ENV
        observed["libraries"] = {}
        for name, (_, version) in LIBRARIES.items():
            paths = ([p for p in base.iterdir() if p.is_dir() and
                      re.sub(r"[^a-z0-9]", "", p.name.lower()) ==
                      re.sub(r"[^a-z0-9]", "", name.lower())] if base.is_dir() else [])
            observed["libraries"][name] = {"present": bool(paths),
                                          "version": package_version(paths[0]) if paths else None,
                                          "required": version}
        observed["data_folder_present"] = (ROOT / "data").is_dir()
        self.log("[DRY-RUN] Read-only cache snapshot complete. Cache absence does not block simulation.")
        if not observed["platformio_cli_detected"]:
            self.log("[DRY-RUN] Observed: PlatformIO CLI not detected; REAL offline build would require it.")

    def dry_fail(self, code: str, stage: str, example: str, suggestion: str) -> int:
        self.report["diagnostics"] = [{
            "code": code, "simulated": True, "stage": stage,
            "examples": [{"text": "[SIMULATED] " + example}],
            "suggestion_vi": suggestion,
        }]
        self.log(f"[DRY-RUN] Injected failure at {stage}: {code}")
        return 2 if stage in ("board_preflight", "dependency_cache") else 1

    def dry_run(self) -> int:
        """Simulate *all* build stages and logs with zero PIO/toolchain execution.

        The one non-simulated gate is a genuine board/source consistency check
        using local Python, allowing faulty GPIOs to be detected even offline.
        No firmware.bin, fake firmware, or placeholder .elf is generated.
        """
        self.log("[DRY-RUN] VQEAF OS: simulation only — not a firmware build")
        self.log("[DRY-RUN] Runs local Python board consistency validation; never runs pio or Xtensa compiler.")
        board_cmd = [sys.executable, "tools/check_board_config.py"]
        rc, out = self.run(board_cmd, "board_preflight.log")
        self.report["board_preflight"] = "PASS_STATIC" if rc == 0 else "FAIL_STATIC"
        self.report["board_check_executed"] = True
        if rc != 0:
            self.report["diagnostics"] = [{
                "code": "BOARD_CONFIG", "simulated": False,
                "examples": [{"text": out[-300:]}],
                "suggestion_vi": "Lỗi thật khi kiểm tra cấu hình board/GPIO. Xem board_preflight.log.",
            }]
            return 2
        if self.args.simulate_failure == "board":
            self.report["board_preflight"] = "SIMULATED_FAIL_AFTER_STATIC_PASS"
            self.dry_step("board_failure_injection", "dry_run_plan.log",
                          "validate board config", "SIMULATED_FAIL", "GPIO mismatch injected only for testing")
            return self.dry_fail("BOARD_CONFIG", "board_preflight", "Wrong TFT pin", "Kiểm tra boards và include/BoardConfig.h.")

        self.dry_probe()
        self.dry_step("dependency_cache", "preflight_simulated.log",
                      "inspect espressif32@6.10.0 + preinstalled Xtensa/Arduino/esptool/SCons + 4 pinned libraries",
                      detail="Predicted version requirements only; see observed_cache. NO packages installed.")
        if self.args.simulate_failure in ("platformio", "toolchain", "library"):
            code = self.args.simulate_failure
            msg = {"platformio": "PlatformIO CLI not found",
                   "toolchain": "toolchain-xtensa-esp32s3 missing in cache",
                   "library": "TFT_eSPI@2.5.43 missing in project libdeps"}[code]
            self.report["dependency_cache"] = "SIMULATED_FAIL"
            return self.dry_fail("OFFLINE_CACHE_INCOMPLETE", "dependency_cache", msg,
                                 "Chuẩn bị cache PlatformIO trên máy có mạng rồi sao chép sang máy offline.")
        self.report["dependency_cache"] = "SIMULATED_PASS_NOT_VERIFIED"

        self.dry_step("platformio_version", "platformio_version.log", "pio --version",
                      detail="Expected: PlatformIO Core CLI already installed; version NOT measured.")
        self.report["platformio_version"] = "SIMULATED_NOT_QUERIED"

        if self.args.check_only:
            self.log("[DRY-RUN] Check-only requested; planned build/clean intentionally skipped.")
            return 0
        if self.args.host_tests:
            self.dry_step("host_tests", "host_cpp.log", f"{sys.executable} tools/test_v14_build.py",
                          detail="Host C++ unit tests would run here; not run in dry-run.")
            self.report["host_tests"] = "SIMULATED_NOT_EXECUTED"

        if self.args.incremental:
            self.report["clean"] = "SKIPPED_INCREMENTAL"
            self.log("[DRY-RUN] Clean skipped (incremental mode).", "steps.log")
        else:
            self.dry_step("clean", "clean.log", f"pio run -e {ENV} -t clean")
            if self.args.simulate_failure == "clean":
                self.report["clean"] = "SIMULATED_FAIL"
                return self.dry_fail("CLEAN_FAILED", "clean", "clean exited 1",
                                     "Xem quyền ghi .pio/build và lỗi clean.log của lần chạy thật.")
            self.report["clean"] = "SIMULATED_PASS"

        raw_errors = {
            "compiler": "src/main.cpp:27:9: error: expected initializer before 'setup'\n",
            "linker": "firmware.elf: undefined reference to `AppRegistry::start()`\ncollect2.exe: error: ld returned 1 exit status\n",
            "network": "ProxyError: Failed to establish a new connection\n",
        }
        err = raw_errors.get(self.args.simulate_failure, "")
        if self.args.simulate_failure == "network":
            self.dry_step("resolve_dependencies", "resolve.log", "offline dependency resolution", "SIMULATED_FAIL",
                          "[SIMULATED] Network request rejected when requested package unavailable")
        else:
            self.dry_step("compile", "compile.log", "xtensa-esp32s3-elf-g++ [project translation units]",
                          "SIMULATED_FAIL" if self.args.simulate_failure == "compiler" else "SIMULATED_PASS",
                          detail=raw_errors.get("compiler", "") if self.args.simulate_failure == "compiler" else
                          "Compilation command illustrative; real commands would be visible in pio -v output.")
            if self.args.simulate_failure != "compiler":
                self.dry_step("link", "link.log", "xtensa-esp32s3-elf-g++ [objects] -o firmware.elf",
                              "SIMULATED_FAIL" if self.args.simulate_failure == "linker" else "SIMULATED_PASS",
                              detail=raw_errors.get("linker", "") if self.args.simulate_failure == "linker" else
                              "Object addresses, linker script and actual symbols unknown in simulation.")
        self.dry_step("target_build", "build.log", f"pio run -e {ENV} -v",
                      "SIMULATED_FAIL" if err else "SIMULATED_PASS",
                      detail=(err or "Would compile/link ESP32-S3 firmware; no tool executed."))
        if err:
            self.report["target_build"] = "SIMULATED_FAIL"
            self.report["diagnostics"] = diagnostic_lines(err)
            for diag in self.report["diagnostics"]:
                diag.update({"simulated": True, "stage": "target_build"})
            return 1
        self.report["target_build"] = "SIMULATED_PASS_NOT_BUILT"
        if self.args.simulate_failure == "missing-bin":
            self.report["firmware_bin"] = "SIMULATED_MISSING"
            self.dry_step("verify_artifact", "artifact_check.log", ".pio/build/vqeaf_os/firmware.bin check",
                          "SIMULATED_FAIL", "firmware.bin does not exist after hypothetical successful build")
            return self.dry_fail("NO_FIRMWARE_BIN", "artifact", "firmware.bin not generated",
                                 "Không dùng firmware giả hoặc firmware cũ; xem log bản build thật.")
        self.dry_step("verify_artifact", "artifact_check.log", ".pio/build/vqeaf_os/firmware.bin size + SHA-256 + freshness",
                      detail="Verification only planned; no file created, no hash invented.")
        self.report["firmware_bin"] = "SIMULATED_NOT_GENERATED"
        self.report["would_generate"] = [f".pio/build/{ENV}/firmware.bin", f".pio/build/{ENV}/firmware.elf"]
        if self.args.buildfs:
            fail = self.args.simulate_failure == "buildfs"
            self.dry_step("buildfs", "buildfs.log", f"pio run -e {ENV} -t buildfs",
                          "SIMULATED_FAIL" if fail else "SIMULATED_PASS",
                          detail="Would require data/ + tool-mklittlefs cache; no filesystem image generated.")
            if fail:
                self.report["buildfs"] = "SIMULATED_FAIL"
                return self.dry_fail("BUILD_FS_FAILED", "buildfs", "buildfs exited 1",
                                     "Kiểm tra thư mục data/ và tool-mklittlefs cache trước build thật.")
            self.report["buildfs"] = "SIMULATED_PASS_NOT_BUILT"
            self.report["would_generate"].append(f".pio/build/{ENV}/littlefs.bin")
        return 0

    def log(self, value: str, file: str = "preflight.log") -> None:
        self.messages.append(value)
        with (self.report_dir / file).open("a", encoding="utf-8") as f:
            f.write(value + "\n")
        print(value, flush=True)

    def run(self, command: List[str], log_name: str) -> Tuple[int, str]:
        display = " ".join([subprocess.list2cmdline([v]) for v in command])
        self.report["commands"].append(display)
        path = self.report_dir / log_name
        raw = ""
        try:
            with path.open("w", encoding="utf-8") as logfile:
                logfile.write(f"# UTC {now()}\n# Command: {display}\n# cwd: {ROOT}\n\n")
                p = subprocess.Popen(command, cwd=str(ROOT), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, encoding="utf-8", errors="replace", env=self.env)
                assert p.stdout is not None
                chunks = []
                for line in p.stdout:
                    logfile.write(line)
                    chunks.append(line)
                    if self.args.verbose:
                        print(line.rstrip("\n"))
                rc = p.wait()
                raw = "".join(chunks)
                logfile.write(f"\n# exit_code={rc}\n")
        except OSError as exc:
            rc = 127
            raw = f"Tool execution failed: {type(exc).__name__}: {exc}\n"
            with path.open("a", encoding="utf-8") as logfile:
                logfile.write(raw + f"# exit_code={rc}\n")
        self.log(f"{'PASS' if rc == 0 else 'FAIL'}: {log_name} (exit {rc})", "steps.log")
        if rc != 0:
            self.report["diagnostics"] = diagnostic_lines(raw)
        return rc, raw

    def check_cache(self) -> bool:
        self.log("VQEAF OS offline preflight — no dependency downloads")
        self.log(f"PlatformIO home: {self.home}")
        self.log("Network guard: HTTP/HTTPS proxy deliberately rejected; use OS network isolation for strict offline enforcement.")
        self.log("Cache is OS-specific; never reuse a Linux cross-toolchain package on Windows.")

        # Verify source configuration before making assumptions about cached libs.
        rc, _ = self.run([sys.executable, "tools/check_board_config.py"], "board_preflight.log")
        self.report["board_preflight"] = "PASS" if rc == 0 else "FAIL"
        if rc != 0:
            self.report["diagnostics"] = [{"code": "BOARD_CONFIG", "suggestion_vi": "Kiểm tra các assert trong board_preflight.log, không tự thay GPIO.", "examples": []}]
            return False

        if not self.pio:
            self.report["missing"].append("PlatformIO CLI (pio/platformio or Python platformio module)")
        if not self.home.exists():
            self.report["missing"].append(f"PlatformIO cache directory: {self.home}")

        platform_dir = self.home / "platforms" / PLATFORM
        platform_json = platform_dir / "platform.json"
        if not platform_json.is_file():
            self.report["missing"].append(str(platform_json))
        else:
            try:
                manifest = json.loads(platform_json.read_text(encoding="utf-8"))
                actual_version = str(manifest.get("version", "UNKNOWN"))
                self.report["cached_platform_version"] = actual_version
                if actual_version != PIN_PLATFORM:
                    self.report["missing"].append(f"Platform mismatch: cached espressif32 {actual_version}; required {PIN_PLATFORM}")
            except (ValueError, OSError) as exc:
                self.report["missing"].append(f"Unreadable {platform_json}: {exc}")

        for name, reason in REQUIRED_PACKAGES.items():
            package = self.home / "packages" / name
            if not package.is_dir() or not (package / "package.json").is_file():
                self.report["missing"].append(f"{package} ({reason})")
                continue
            installed = package_version(package) or "UNKNOWN"
            self.report.setdefault("cached_packages", {})[name] = installed
            version_prefix = {"toolchain-xtensa-esp32s3": "8.4.0",
                              "framework-arduinoespressif32": "3.20017.",
                              "tool-esptoolpy": "1.40501."}.get(name)
            if version_prefix and not installed.startswith(version_prefix):
                self.report["missing"].append(f"Wrong {name} version: {installed}; expected {version_prefix}* for espressif32@6.10.0")
            if name == "toolchain-xtensa-esp32s3":
                exe = package / "bin" / ("xtensa-esp32s3-elf-g++.exe" if os.name == "nt" else "xtensa-esp32s3-elf-g++")
                if not exe.is_file():
                    self.report["missing"].append(f"Compiler missing: {exe}")
            if name == "framework-arduinoespressif32":
                if not (package / "cores" / "esp32" / "Arduino.h").is_file():
                    self.report["missing"].append(f"Arduino.h missing: {package / 'cores/esp32/Arduino.h'}")

        if self.args.buildfs:
            if not (ROOT / "data").is_dir():
                self.report["missing"].append("data/ (LittleFS source folder missing from v2.3.3; --buildfs only if you created/populated it)")
            p = self.home / "packages" / "tool-mklittlefs"
            if not p.is_dir() or not (p / "package.json").is_file():
                self.report["missing"].append(f"{p} (--buildfs requested)")

        # The project has pinned lib_deps. Require them to already exist; a
        # missing library must fail BEFORE calling pio run (which might install).
        for folder_name, (lib, version) in LIBRARIES.items():
            # lib_deps are registry-style pinned dependencies, not lib_extra_dirs.
            # Requiring the project's installed libdeps prevents PIO resolving them
            # online even when an unrelated globally installed library exists.
            base = ROOT / ".pio" / "libdeps" / ENV
            found: Optional[Path] = None
            if base.is_dir():
                for child in base.iterdir():
                    if not child.is_dir():
                        continue
                    normalized = re.sub(r"[^a-z0-9]", "", child.name.lower())
                    wanted = re.sub(r"[^a-z0-9]", "", lib.lower())
                    if normalized != wanted:
                        continue
                    v = package_version(child)
                    if v == version:
                        found = child
                        break
                    self.report["warnings"].append(f"Found {child} version {v or '?'}; required {version}.")
            if not found:
                self.report["missing"].append(f"Pinned lib: {lib}@{version} (must be installed in .pio/libdeps/{ENV})")
            else:
                self.report.setdefault("cached_libraries", {})[folder_name] = str(found)

        if self.report["missing"]:
            self.report["dependency_cache"] = "FAIL"
            for item in self.report["missing"]:
                self.log("MISSING: " + item)
            self.report["diagnostics"] = [{
                "code": "OFFLINE_CACHE_INCOMPLETE", "suggestion_vi":
                "Trên máy có internet, chạy 'pio pkg install -e vqeaf_os' trong dự án và một lần 'pio run -e vqeaf_os'. "
                "Sau đó sao chép thư mục .platformio (cùng hệ điều hành) cùng .pio/libdeps/vqeaf_os; "
                "máy offline KHÔNG tự tải thư viện.", "examples": [{"text": x} for x in self.report["missing"][:10]],
            }]
            return False

        rc, out = self.run(self.pio + ["--version"], "platformio_version.log")  # type: ignore[operator]
        if rc != 0:
            self.report["missing"].append("Installed PlatformIO CLI cannot run --version")
            self.report["dependency_cache"] = "FAIL"
            return False
        self.report["platformio_version"] = out.strip().splitlines()[-1] if out.strip() else "UNKNOWN"
        self.report["dependency_cache"] = "PASS"
        self.log("PASS: cached PlatformIO, ESP32-S3 compiler, Arduino, esptool, and all 4 pinned libraries")
        return True

    def artifact(self, path: Path) -> None:
        if path.exists() and path.stat().st_size:
            self.report["artifacts"].append({"path": str(path.relative_to(ROOT)), "size_bytes": path.stat().st_size, "sha256": sha256(path)})

    def build(self) -> int:
        # All operations that may invoke the package manager happen ONLY after
        # preflight checks. The rejected proxies fail any incidental HTTP requests.
        if self.args.host_tests:
            if shutil.which("g++"):
                rc, _ = self.run([sys.executable, "tools/test_v14_build.py"], "host_cpp.log")
                self.report["host_tests"] = "PASS" if rc == 0 else "FAIL"
                if rc != 0:
                    return 1
            else:
                self.report["host_tests"] = "SKIPPED_NO_GXX"
                self.report["warnings"].append("Native g++ missing; target build will still proceed.")

        if not self.args.incremental:
            rc, _ = self.run(self.pio + ["run", "-e", ENV, "-t", "clean"], "clean.log")  # type: ignore[operator]
            self.report["clean"] = "PASS" if rc == 0 else "FAIL"
            if rc != 0:
                return 1
        else:
            self.report["clean"] = "SKIPPED_INCREMENTAL"

        started = datetime.now(timezone.utc)
        rc, out = self.run(self.pio + ["run", "-e", ENV, "-v"], "build.log")  # type: ignore[operator]
        if rc != 0:
            self.report["target_build"] = "FAIL"
            return 1
        self.report["target_build"] = "PASS"
        artifacts = ROOT / ".pio" / "build" / ENV
        firmware = artifacts / "firmware.bin"
        if not firmware.is_file() or firmware.stat().st_size == 0:
            self.report["firmware_bin"] = "MISSING_AFTER_BUILD"
            self.report["diagnostics"] = [{"code": "NO_FIRMWARE_BIN", "examples": [], "suggestion_vi": "pio báo exit 0 nhưng firmware.bin không tồn tại; kiểm tra env và build.log."}]
            return 1
        if self.args.incremental:
            # In incremental mode an unchanged, previously built binary is valid
            # only when all source/config inputs are older than the binary.
            sources = [ROOT / "platformio.ini"]
            for d in ("src", "include", "boards", "partitions"):
                sources.extend(p for p in (ROOT / d).rglob("*") if p.is_file())
            newest = max((p.stat().st_mtime for p in sources), default=0)
            stale = firmware.stat().st_mtime < newest
        else:
            stale = firmware.stat().st_mtime < (started.timestamp() - 3)
        if stale:
            self.report["firmware_bin"] = "STALE_AFTER_BUILD"
            self.report["diagnostics"] = [{"code": "STALE_FIRMWARE", "examples": [], "suggestion_vi": "firmware.bin cũ hoặc không mới hơn source; dùng chế độ clean mặc định và kiểm tra đường dẫn build."}]
            return 1
        self.report["firmware_bin"] = "GENERATED"
        for name in ("firmware.bin", "firmware.elf", "bootloader.bin", "partitions.bin"):
            self.artifact(artifacts / name)
        self.log(f"PASS: firmware.bin fresh, size {firmware.stat().st_size} bytes, SHA256 {sha256(firmware)}", "steps.log")

        if self.args.buildfs:
            rc, _ = self.run(self.pio + ["run", "-e", ENV, "-t", "buildfs"], "buildfs.log")  # type: ignore[operator]
            self.report["buildfs"] = "PASS" if rc == 0 else "FAIL"
            if rc != 0:
                return 1
            # Some PlatformIO versions use littlefs.bin; check both names and
            # never report generated without real nonempty artifact.
            fs = artifacts / "littlefs.bin"
            if not fs.exists() or not fs.stat().st_size:
                self.report["buildfs"] = "FAIL_NO_LITTLEFS_BIN"
                self.report["diagnostics"] = [{"code": "NO_FS_BIN", "examples": [], "suggestion_vi": "buildfs returned success but littlefs.bin is missing."}]
                return 1
            self.artifact(fs)
        return 0

    def finalize(self, rc: int) -> None:
        if self.args.dry_run:
            if self.report["board_preflight"] == "FAIL_STATIC":
                self.report["status"] = "DRY_RUN_STATIC_BOARD_FAILURE"
            elif rc != 0:
                self.report["status"] = "DRY_RUN_SIMULATED_FAILURE"
            elif self.args.check_only:
                self.report["status"] = "DRY_RUN_CHECK_ONLY"
            else:
                self.report["status"] = "DRY_RUN_COMPLETE"
            # A completed dry-run is not proof of build, dependencies or a bin.
            self.report["firmware_created"] = False
            self.report["artifacts"] = []
            plan = [
                "# VQEAF OS - DRY RUN EXECUTION PLAN (NO CROSS COMPILATION)",
                "Local Python board preflight was run; everything below was simulated.",
            ]
            for stage in self.report["simulated_steps"]:
                plan.append(f"{stage['step']}: {stage['result']}: {stage['command']}")
            (self.report_dir / "dry_run_plan.log").write_text("\n".join(plan) + "\n", encoding="utf-8")
        elif rc == 0 and self.args.check_only and self.report["dependency_cache"] == "PASS":
            self.report["status"] = "READY_NO_BUILD"
        elif rc == 0 and self.report["firmware_bin"] == "GENERATED" and self.report["target_build"] == "PASS" and self.report["buildfs"] not in ("FAIL", "FAIL_NO_LITTLEFS_BIN"):
            self.report["status"] = "SUCCESS"
        elif self.report["board_preflight"] != "PASS" or self.report["dependency_cache"] != "PASS":
            self.report["status"] = "BLOCKED_PREFLIGHT"
        else:
            self.report["status"] = "FAILED_BUILD"
        self.report["finished_utc"] = now()
        self.report["exit_code"] = rc
        (self.report_dir / "report.json").write_text(json.dumps(self.report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        md = [
            "# VQEAF OS v2.3.3 — Offline PlatformIO build report", "",
            f"**UTC:** {self.report['timestamp_utc']}  ", f"**Status:** {self.report['status']}  ",
            f"**PlatformIO home:** `{self.home}`  ", "",
            "| Step | Result |", "|---|---|",
        ]
        if self.args.dry_run:
            md[0] = "# VQEAF OS v2.3.3 — DRY-RUN SIMULATION REPORT"
            md[4:4] = [
                "", "**SIMULATION ONLY — NO ESP32-S3 FIRMWARE WAS BUILT, NO PLATFORMIO OR TOOLCHAIN RAN.**",
                "Only local Python GPIO/board/source consistency preflight really executed.",
                "No firmware.bin generated, no firmware hash estimated, no actual cache verified.",
                "",
            ]
        for key in ("board_preflight", "dependency_cache", "platformio_version", "clean", "host_tests", "target_build", "firmware_bin", "buildfs"):
            md.append(f"| {key} | {self.report[key]} |")
        md += ["", ("## Simulated preflight errors (not a real cache audit)" if self.args.dry_run
                    else "## Missing offline dependencies"), ""]
        md.extend("- " + x for x in self.report["missing"] or ["None"])
        md += ["", "## Error diagnostics / suggested fixes", ""]
        for d in self.report["diagnostics"]:
            md.append("### " + d["code"])
            md.append(d["suggestion_vi"] + "\n")
            md.extend("- " + (str(e.get("line", "")) + ": ") + e.get("text", "") for e in d["examples"])
        if not self.report["diagnostics"]:
            md.append("None")
        md += ["", "## Artifacts", ""]
        md.extend(f"- `{a['path']}` — {a['size_bytes']} bytes; SHA-256 `{a['sha256']}`" for a in self.report["artifacts"])
        if not self.report["artifacts"]:
            md.append("- No build artifacts generated or verified" if self.args.dry_run else "- No build artifacts verified")
        if self.args.dry_run:
            md += ["", "## Simulated pipeline (NEVER EXECUTED)", "",
                   "| Stage | Planned command | Simulated result |", "|---|---|---|"]
            for stage in self.report["simulated_steps"]:
                md.append("| {step} | `{command}` | {result} |".format(**stage))
            md += ["", "## Read-only environment observations (NOT proof of readiness)", "",
                   "```json", json.dumps(self.report["observed_cache"], ensure_ascii=False, indent=2),
                   "```", "", "## Hypothetical outputs", ""]
            md.extend("- `" + name + "` — **NOT GENERATED**" for name in self.report["would_generate"])
            if not self.report["would_generate"]:
                md.append("- None; pipeline stopped before artifact stage")
        md += ["", "## Logs and reproducibility", "",
               ("`board_preflight.log` (REAL STATIC CHECK), `preflight.log`, `steps.log`, `dry_run_plan.log`, "
                "`preflight_simulated.log`, `platformio_version.log`, `clean.log`, `compile.log`, `link.log`, `build.log`, "
                "`artifact_check.log`; `buildfs.log` and `host_cpp.log` when requested."
                if self.args.dry_run else
                "`board_preflight.log`, `preflight.log`, `steps.log`, `platformio_version.log`, `clean.log`, `build.log` and optionally `buildfs.log`, `host_cpp.log`."),
               "", ("Run `tools/build_offline.py` without `--dry-run` on a provisioned machine for a REAL build."
                     if self.args.dry_run else
                     "Offline mode uses existing caches and a rejecting HTTP proxy. It does **not** prevent custom code or tools from opening raw sockets; disconnect the network or use a firewall for strict isolation."),
               "A passing dry-run or host test never means a successful ESP32-S3 firmware build or hardware test.", ""]
        (self.report_dir / "report.md").write_text("\n".join(md), encoding="utf-8")
        print(f"\nResult: {self.report['status']} (exit {rc})")
        print(f"Report: {self.report_dir / 'report.md'}")
        print(f"JSON:   {self.report_dir / 'report.json'}")
        if self.report["target_build"] == "FAIL":
            print(f"Build failure log: {self.report_dir / 'build.log'}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build VQEAF OS from PREINSTALLED PlatformIO cache without downloading packages")
    parser.add_argument("--pio", help="Full path to installed pio/platformio CLI (if not in PATH)")
    parser.add_argument("--pio-home", help="PLATFORMIO_CORE_DIR; default environment variable or ~/.platformio")
    parser.add_argument("--report-dir", help="Output reports directory; default build_reports/offline")
    parser.add_argument("--check-only", action="store_true", help="Verify toolchain/library caches but do not build or claim firmware.bin")
    parser.add_argument("--dry-run", action="store_true", help="Simulate preflight/clean/compile/link/artifact (+ optional buildfs) without PlatformIO/toolchain; real static board check only")
    parser.add_argument("--simulate-failure", choices=DRY_RUN_FAILURES, default="none",
                        help="Inject synthetic error for testing diagnostics (requires --dry-run)")
    parser.add_argument("--buildfs", action="store_true", help="Also build LittleFS; requires populated data/ and cached tool-mklittlefs")
    parser.add_argument("--host-tests", action="store_true", help="Run optional host g++ test before cross-build")
    parser.add_argument("--incremental", action="store_true", help="Skip clean; default is fresh clean build")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print complete PlatformIO output as it runs")
    args = parser.parse_args()
    if not args.dry_run and args.simulate_failure != "none":
        parser.error("--simulate-failure requires --dry-run")
    if args.simulate_failure == "buildfs" and not args.buildfs:
        parser.error("--simulate-failure buildfs requires --buildfs")
    if args.simulate_failure == "clean" and args.incremental:
        parser.error("--simulate-failure clean is incompatible with --incremental")
    if args.check_only and args.simulate_failure not in ("none", "board", "platformio", "toolchain", "library"):
        parser.error("--check-only can inject only board/platformio/toolchain/library failures")
    build = Build(args)
    code = 3
    try:
        if args.dry_run:
            code = build.dry_run()
        else:
            code = 0 if build.check_cache() else 2
            if code == 0 and not args.check_only:
                code = build.build()
    except KeyboardInterrupt:
        code = 130
        build.report["diagnostics"] = [{"code": "CANCELLED", "examples": [], "suggestion_vi": "Build bị người dùng dừng (Ctrl+C)."}]
    except Exception as e:
        code = 3
        details = traceback.format_exc()
        (build.report_dir / "internal_error.log").write_text(details, encoding="utf-8")
        build.report["diagnostics"] = [{"code": "SCRIPT_ERROR", "examples": [{"text": str(e)}], "suggestion_vi": "Mở internal_error.log; đây là lỗi script, chưa có kết quả build hợp lệ."}]
    finally:
        build.finalize(code)
    return code


if __name__ == "__main__":
    sys.exit(main())
