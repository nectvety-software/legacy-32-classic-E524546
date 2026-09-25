#!/usr/bin/env python3
"""Unit tests for OFFLINE BUILD ORCHESTRATION; uses a FAKE PIO, not MCU compiler.

Does not install dependencies or make network calls. Passing is not a real
PlatformIO target build. Run: python tools/test_offline_build.py
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent.parent


class OfflineBuildTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="vqeaf_offline_mock_")
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.project = self.base / "VQEAF-OS"
        self.project.mkdir()
        # Board preflight runs unmodified against test fixture copies.
        for item in ("tools/build_offline.py", "tools/check_board_config.py", "platformio.ini",
                     "boards/vqeaf_s3_n16r8.json", "partitions/vqeaf_16mb_ota.csv",
                     "include/BoardConfig.h", "src/main.cpp", "src/core/SymbianUI.cpp"):
            target = self.project / item
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(HERE / item, target)
        self.home = self.base / "pio_cache"
        self.pio = self.base / "fake_pio.py"
        self.pio.write_text('''#!/usr/bin/env python3
import os,sys
from pathlib import Path
root=Path.cwd();a=sys.argv[1:]
if os.environ.get('FAKE_PIO_TRACE_FILE'):
    with open(os.environ['FAKE_PIO_TRACE_FILE'],'a',encoding='utf-8') as f:f.write('INVOKED: '+str(a)+'\\n')
if '--version' in a: print('PlatformIO Core, version 6.1.18 [TEST DOUBLE]');sys.exit(0)
if '-t' in a:
    t=a[a.index('-t')+1]
    if t=='clean':
        import shutil
        shutil.rmtree(root/'.pio/build/vqeaf_os',ignore_errors=True)
        print('Cleaned fake build')
        sys.exit(0)
    if t=='buildfs':
        p=root/'.pio/build/vqeaf_os/littlefs.bin';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'fake_littlefs')
        sys.exit(0)
if os.environ.get('FAKE_PIO_FAIL'):
    print('src/main.cpp:15:10: fatal error: MissingLib.h: No such file or directory');sys.exit(1)
if not os.environ.get('FAKE_PIO_NO_BIN'):
    p=root/'.pio/build/vqeaf_os/firmware.bin';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'FAKE_IMAGE_NOT_FLASHABLE')
    p.with_suffix('.elf').write_bytes(b'FAKE_ELF')
print('FAKE PIO success, not a firmware test')
''', encoding="utf-8")
        if os.name != 'nt':
            self.pio.chmod(0o755)

    def create_cache(self):
        p = self.home / "platforms/espressif32"
        p.mkdir(parents=True)
        (p / "platform.json").write_text(json.dumps({"version": "6.10.0"}), encoding="utf-8")
        for name, version in {"toolchain-xtensa-esp32s3": "8.4.0+2021r2-patch5",
                              "framework-arduinoespressif32": "3.20017.0",
                              "tool-esptoolpy": "1.40501.0",
                              "tool-scons": "4.40700.0", "tool-mklittlefs": "1.203.210628"}.items():
            p = self.home / "packages" / name
            p.mkdir(parents=True)
            (p / "package.json").write_text(json.dumps({"version": version}), encoding="utf-8")
        cross = self.home / "packages/toolchain-xtensa-esp32s3/bin"
        cross.mkdir()
        (cross / ("xtensa-esp32s3-elf-g++.exe" if os.name == "nt" else "xtensa-esp32s3-elf-g++")).write_bytes(b"TEST_PLACEHOLDER")
        h = self.home / "packages/framework-arduinoespressif32/cores/esp32"
        h.mkdir(parents=True)
        (h / "Arduino.h").write_text("// fake")
        for name, version in {"TFT_eSPI": "2.5.43", "NimBLE-Arduino": "2.3.6",
                              "TJpg_Decoder": "1.1.0", "PNGdec": "1.1.6"}.items():
            p = self.project / ".pio/libdeps/vqeaf_os" / name
            p.mkdir(parents=True)
            (p / "library.json").write_text(json.dumps({"version": version}), encoding="utf-8")

    def run_build(self, *args, env_extra=None):
        env = os.environ.copy()
        if env_extra:
            env.update(env_extra)
        argv = [sys.executable, str(self.project / "tools/build_offline.py"), "--pio", str(self.pio),
                "--pio-home", str(self.home), "--report-dir", str(self.base / "reports")] + list(args)
        p = subprocess.run(argv, cwd=self.project, capture_output=True, text=True, env=env)
        d = json.loads((self.base / "reports/report.json").read_text(encoding="utf-8"))
        return p, d

    def test_incomplete_cache_blocks_without_attempt(self):
        p, report = self.run_build()
        self.assertEqual(p.returncode, 2, p.stdout + p.stderr)
        self.assertEqual(report["status"], "BLOCKED_PREFLIGHT")
        self.assertEqual(report["target_build"], "NOT_RUN")
        self.assertIn("OFFLINE_CACHE_INCOMPLETE", [d["code"] for d in report["diagnostics"]])
        self.assertFalse((self.project / ".pio/build/vqeaf_os/firmware.bin").exists())

    def test_complete_cache_check_only(self):
        self.create_cache()
        p, report = self.run_build("--check-only")
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(report["status"], "READY_NO_BUILD")
        self.assertEqual(report["target_build"], "NOT_RUN")
        self.assertEqual(report["firmware_bin"], "NOT_GENERATED")

    def test_mock_success_creates_sha_and_clean(self):
        self.create_cache()
        p, report = self.run_build()
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(report["status"], "SUCCESS")
        self.assertEqual(report["clean"], "PASS")
        self.assertEqual(report["target_build"], "PASS")
        self.assertEqual(report["firmware_bin"], "GENERATED")
        self.assertGreater(report["artifacts"][0]["size_bytes"], 0)

    def test_compiler_failure_classified(self):
        self.create_cache()
        p, report = self.run_build(env_extra={"FAKE_PIO_FAIL": "1"})
        self.assertEqual(p.returncode, 1)
        self.assertEqual(report["status"], "FAILED_BUILD")
        self.assertIn("MISSING_LIBRARY", [d["code"] for d in report["diagnostics"]])
        self.assertEqual(report["firmware_bin"], "NOT_GENERATED")

    def test_success_without_binary_is_failure(self):
        self.create_cache()
        p, report = self.run_build(env_extra={"FAKE_PIO_NO_BIN": "1"})
        self.assertEqual(p.returncode, 1)
        self.assertEqual(report["firmware_bin"], "MISSING_AFTER_BUILD")
        self.assertEqual(report["status"], "FAILED_BUILD")

    def test_wrong_platform_version_prevents_build(self):
        self.create_cache()
        (self.home / "platforms/espressif32/platform.json").write_text('{"version":"6.9.0"}')
        p, report = self.run_build()
        self.assertEqual(p.returncode, 2)
        self.assertTrue(any("Platform mismatch" in m for m in report["missing"]))
        self.assertEqual(report["target_build"], "NOT_RUN")

    def test_mismatched_library_prevents_build(self):
        self.create_cache()
        (self.project / ".pio/libdeps/vqeaf_os/PNGdec/library.json").write_text('{"version":"0.9.0"}')
        p, report = self.run_build()
        self.assertEqual(p.returncode, 2)
        self.assertTrue(any("PNGdec" in m for m in report["missing"]))

    def test_buildfs_missing_data_reports_clearly(self):
        self.create_cache()
        p, report = self.run_build("--buildfs")
        self.assertEqual(p.returncode, 2)
        self.assertTrue(any("data/" in m for m in report["missing"]))

    def test_mock_buildfs_success(self):
        self.create_cache()
        (self.project / "data").mkdir()
        (self.project / "data/readme.txt").write_text("test")
        p, report = self.run_build("--buildfs")
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(report["buildfs"], "PASS")
        self.assertIn("littlefs.bin", [Path(a["path"]).name for a in report["artifacts"]])

    def test_wrong_framework_version_prevents_build(self):
        self.create_cache()
        (self.home / "packages/framework-arduinoespressif32/package.json").write_text('{"version":"3.30000.0"}')
        p, report = self.run_build()
        self.assertEqual(p.returncode, 2)
        self.assertTrue(any("Wrong framework" in m for m in report["missing"]))

    # Dry-run tests deliberately DO NOT create any cache or install pio.
    # FAKE_PIO_TRACE_FILE proves the CLI was never invoked (even --version).
    def dry_build(self, *args):
        marker = self.base / 'PIO_MUST_NOT_RUN.txt'
        result = self.run_build('--dry-run', *args,
                                env_extra={'FAKE_PIO_TRACE_FILE': str(marker)})
        self.assertFalse(marker.exists(), 'dry-run launched PlatformIO')
        self.assertFalse((self.project/'.pio/build/vqeaf_os/firmware.bin').exists(),
                         'dry-run generated a fake flashable binary')
        return result

    def test_dry_run_without_any_cache_or_toolchain(self):
        p, report = self.dry_build()
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(report['status'], 'DRY_RUN_COMPLETE')
        self.assertEqual(report['board_preflight'], 'PASS_STATIC')
        self.assertEqual(report['target_build'], 'SIMULATED_PASS_NOT_BUILT')
        self.assertEqual(report['firmware_bin'], 'SIMULATED_NOT_GENERATED')
        self.assertFalse(report['firmware_created'])
        self.assertEqual(report['artifacts'], [])
        self.assertIn('platformio_cli_detected', report['observed_cache'])
        self.assertEqual(len(report['simulated_steps']), 7)
        for name in ('board_preflight.log', 'preflight.log', 'steps.log',
                     'dry_run_plan.log', 'report.md', 'report.json', 'clean.log',
                     'build.log', 'platformio_version.log', 'artifact_check.log'):
            self.assertTrue((self.base/'reports'/name).exists(), name)

    def test_dry_run_buildfs_and_host_tests_always_simulated(self):
        p, report = self.dry_build('--buildfs', '--host-tests')
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(report['buildfs'], 'SIMULATED_PASS_NOT_BUILT')
        self.assertEqual(report['host_tests'], 'SIMULATED_NOT_EXECUTED')
        self.assertFalse(report['observed_cache']['data_folder_present'])
        self.assertEqual(len(report['would_generate']), 3)
        self.assertTrue((self.base/'reports'/'buildfs.log').exists())
        self.assertTrue((self.base/'reports'/'host_cpp.log').exists())

    def test_dry_run_check_only_stops_before_build(self):
        p, report = self.dry_build('--check-only')
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(report['status'], 'DRY_RUN_CHECK_ONLY')
        self.assertEqual(report['target_build'], 'NOT_RUN')
        self.assertEqual(report['clean'], 'NOT_RUN')
        self.assertFalse((self.base/'reports'/'build.log').exists())

    def test_dry_run_simulates_dependency_failure(self):
        p, report = self.dry_build('--simulate-failure', 'toolchain')
        self.assertEqual(p.returncode, 2, p.stdout + p.stderr)
        self.assertEqual(report['status'], 'DRY_RUN_SIMULATED_FAILURE')
        self.assertIn('OFFLINE_CACHE_INCOMPLETE', [d['code'] for d in report['diagnostics']])
        self.assertEqual(report['target_build'], 'NOT_RUN')

    def test_dry_run_simulated_compiler_and_linker_failures(self):
        for category, expected in [('compiler', 'COMPILER'), ('linker', 'LINKER')]:
            # New output directory avoids stale report artifacts between cases.
            p, report = self.dry_build('--simulate-failure', category)
            self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
            self.assertEqual(report['status'], 'DRY_RUN_SIMULATED_FAILURE')
            self.assertIn(expected, [d['code'] for d in report['diagnostics']])
            if category == 'linker':
                self.assertNotIn('COMPILER', [d['code'] for d in report['diagnostics']],
                                 'linker-only failure must not be mislabeled as syntax error')
            self.assertTrue(all(d.get('simulated') for d in report['diagnostics']))

    def test_dry_run_no_firmware_produces_specific_error(self):
        p, report = self.dry_build('--simulate-failure', 'missing-bin')
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertEqual(report['firmware_bin'], 'SIMULATED_MISSING')
        self.assertIn('NO_FIRMWARE_BIN', [d['code'] for d in report['diagnostics']])

    def test_dry_run_simulated_buildfs_error(self):
        p, report = self.dry_build('--buildfs', '--simulate-failure', 'buildfs')
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertEqual(report['buildfs'], 'SIMULATED_FAIL')
        self.assertIn('BUILD_FS_FAILED', [d['code'] for d in report['diagnostics']])

    def test_dry_run_actual_static_board_failure_is_not_simulated(self):
        path = self.project/'include/BoardConfig.h'
        content = path.read_text(encoding='utf-8')
        import re
        self.assertRegex(content, r'TFT_SCL_PIN\s*=\s*48')
        path.write_text(re.sub(r'TFT_SCL_PIN\s*=\s*48', 'TFT_SCL_PIN = 22', content), encoding='utf-8')
        p, report = self.dry_build()
        self.assertEqual(p.returncode, 2, p.stdout + p.stderr)
        self.assertEqual(report['status'], 'DRY_RUN_STATIC_BOARD_FAILURE')
        self.assertEqual(report['board_preflight'], 'FAIL_STATIC')
        self.assertIn('BOARD_CONFIG', [d['code'] for d in report['diagnostics']])
        self.assertFalse(report['diagnostics'][0]['simulated'])

    def test_dry_run_incremental_skips_clean_without_runtime(self):
        p, report = self.dry_build('--incremental')
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(report['clean'], 'SKIPPED_INCREMENTAL')
        self.assertFalse((self.base/'reports'/'clean.log').exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
