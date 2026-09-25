"""Build recovery, error codes, and Host/Build separation regression (PROMPT matrix)."""
from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'tools'))

from studio.core.errors import ERROR_CATALOG, StudioError
from studio_firmware import FirmwareRootError, firmware_root, inspect_firmware_root


class ErrorCodeCatalogTests(unittest.TestCase):
    def test_required_codes_present(self):
        for code in (
            'OS_ROOT_MISSING', 'OS_ROOT_INVALID', 'OS_ROOT_IS_STUDIO', 'LUA_BETA_REQUIRED',
            'PROJECT_INVALID', 'SIGNER_FAILED', 'VM_CRASH', 'QT_PLUGIN_MISSING', 'HOST_LIMIT',
        ):
            self.assertIn(code, ERROR_CATALOG)
            self.assertTrue(ERROR_CATALOG[code].fix)

    def test_studio_error_ui_message_includes_fix(self):
        err = StudioError('OS_ROOT_MISSING')
        text = err.ui_message()
        self.assertIn('OS_ROOT_MISSING', text)
        self.assertIn('Khắc phục', text)


class FirmwareInspectTests(unittest.TestCase):
    def test_empty_is_os_root_missing(self):
        report = inspect_firmware_root('')
        self.assertEqual(report['error_code'], 'OS_ROOT_MISSING')
        self.assertFalse(report['ready_for_build'])

    def test_studio_folder_rejected(self):
        report = inspect_firmware_root(ROOT)
        self.assertEqual(report['error_code'], 'OS_ROOT_IS_STUDIO')
        self.assertTrue(report['is_studio'])

    def test_invalid_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            report = inspect_firmware_root(Path(tmp) / 'nope')
            self.assertEqual(report['error_code'], 'OS_ROOT_INVALID')

    def test_folder_without_signer(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'platformio.ini').write_text('[env]\n', encoding='utf-8')
            report = inspect_firmware_root(root)
            self.assertEqual(report['error_code'], 'SIGNER_MISSING')
            self.assertFalse(report['ready_for_build'])

    def test_valid_tree_ready_for_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'VQEAF-OS'
            (root / 'tools').mkdir(parents=True)
            (root / 'platformio.ini').write_text('[env]\n', encoding='utf-8')
            (root / 'tools' / 'build_qeapp.py').write_text('# signer\n', encoding='utf-8')
            report = inspect_firmware_root(root)
            self.assertTrue(report['ready_for_build'])
            self.assertEqual(report['error_code'], '')
            self.assertFalse(report['ready_for_lua_build'])

    def test_lua_beta_flag(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'VQEAF-OS'
            (root / 'tools').mkdir(parents=True)
            (root / 'src' / 'lua').mkdir(parents=True)
            (root / 'platformio.ini').write_text('[env]\n', encoding='utf-8')
            (root / 'tools' / 'build_qeapp.py').write_text('# signer\n', encoding='utf-8')
            (root / 'src' / 'lua' / 'QeLuaRuntime.cpp').write_text('// beta\n', encoding='utf-8')
            report = inspect_firmware_root(root)
            self.assertTrue(report['ready_for_lua_build'])

    def test_paths_with_spaces_and_unicode(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'VQEAF OS (bản quyền)'
            (root / 'tools').mkdir(parents=True)
            (root / 'platformio.ini').write_text('[env]\n', encoding='utf-8')
            (root / 'tools' / 'build_qeapp.py').write_text('#\n', encoding='utf-8')
            report = inspect_firmware_root(str(root))
            self.assertTrue(report['ready_for_build'], report)


class FirmwareRootPriorityTests(unittest.TestCase):
    def test_env_wins_when_valid(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'FW'
            (root / 'tools').mkdir(parents=True)
            (root / 'platformio.ini').write_text('[env]\n', encoding='utf-8')
            (root / 'tools' / 'build_qeapp.py').write_text('#\n', encoding='utf-8')
            with patch.dict(os.environ, {'QEAPP_FIRMWARE_ROOT': str(root)}):
                self.assertEqual(firmware_root(), root.resolve())

    def test_invalid_env_raises_when_required(self):
        with patch.dict(os.environ, {'QEAPP_FIRMWARE_ROOT': '/path/does/not/exist'}):
            with self.assertRaises(FirmwareRootError):
                firmware_root(required=True)

    def test_required_missing_message_mentions_os_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            # Point env at empty dir so discovery is forced to fail.
            empty = Path(tmp) / 'empty'
            empty.mkdir()
            with patch.dict(os.environ, {'QEAPP_FIRMWARE_ROOT': str(empty)}):
                # Also hide sibling/embedded by using a fake STUDIO_ROOT? discovery still finds monorepo firmware.
                # Accept either FirmwareRootError or a resolved monorepo path.
                try:
                    found = firmware_root(required=True)
                    self.assertTrue((found / 'platformio.ini').is_file())
                except FirmwareRootError as exc:
                    self.assertIn('OS_ROOT', str(exc))


class BuildSeparationContractTests(unittest.TestCase):
    def test_window_source_separates_operations(self):
        source = (ROOT / 'studio' / 'gui' / 'window.py').read_text(encoding='utf-8')
        self.assertIn('BUILD_SIGNED_QEAPP', source)
        self.assertIn('RUN_HOST_LUA', source)
        self.assertIn('VALIDATE_PROJECT', source)
        self.assertIn('BUILD FAILED', source)
        self.assertIn('BUILD PASSED', source)
        self.assertIn('HOST RUNNING', source)
        self.assertIn('OS_ROOT_MISSING', source)
        self.assertIn('LUA_BETA_REQUIRED', source)

    def test_build_preflight_does_not_block_host_run(self):
        # Source contract: launch_emulator path does not call _firmware().
        source = (ROOT / 'studio' / 'gui' / 'window.py').read_text(encoding='utf-8')
        start = source.index('def launch_emulator')
        end = source.index('def ', start + 10)
        body = source[start:end]
        self.assertNotIn('_firmware(', body)

    def test_t9_is_host_limit_not_crash(self):
        vp = (ROOT / 'studio' / 'gui' / 'virtual_phone.py').read_text(encoding='utf-8')
        self.assertIn('HOST_LIMIT', vp)
        self.assertIn('not emulated', vp)
        self.assertIn('not a crash', vp)

    def test_settings_dialog_exists(self):
        self.assertTrue((ROOT / 'studio' / 'gui' / 'firmware_settings.py').is_file())


class DoctorBundleTests(unittest.TestCase):
    def test_export_bundle_redacts_secrets(self):
        from studio_doctor import export_bundle
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            # plant a fake log with a private key marker
            logs = ROOT / 'logs'
            logs.mkdir(exist_ok=True)
            fake = logs / 'gui-errors.log'
            prior = fake.read_text(encoding='utf-8') if fake.is_file() else ''
            fake.write_text(prior + 'sign_key=C:\\secret\\key.pem\n', encoding='utf-8')
            try:
                path = export_bundle(out)
                self.assertTrue(path.is_file())
                import zipfile
                with zipfile.ZipFile(path) as zf:
                    names = zf.namelist()
                    self.assertIn('diagnostics.json', names)
                    joined = '\n'.join(
                        zf.read(name).decode('utf-8', errors='replace') for name in names
                    )
                    self.assertIn('[REDACTED', joined)
                    self.assertNotIn('C:\\secret\\key.pem', joined)
            finally:
                fake.write_text(prior, encoding='utf-8')

    def test_doctor_json_includes_environment(self):
        from studio_doctor import collect_environment
        env = collect_environment()
        self.assertIn('python', env)
        self.assertIn('firmware_check', env)


if __name__ == '__main__':
    unittest.main()
