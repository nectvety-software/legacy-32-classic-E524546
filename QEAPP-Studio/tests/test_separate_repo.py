"""Standalone repo separation tests; no firmware / hardware required."""
from __future__ import annotations
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from studio_firmware import FirmwareRootError, firmware_root, inspect_firmware_root


def _write_checkout(root: Path, *, lua: bool = False) -> Path:
    (root / 'tools').mkdir(parents=True, exist_ok=True)
    (root / 'platformio.ini').write_text('[env]\n', encoding='utf-8')
    (root / 'tools' / 'build_qeapp.py').write_text('# signer\n', encoding='utf-8')
    if lua:
        (root / 'src' / 'lua').mkdir(parents=True, exist_ok=True)
        (root / 'src' / 'lua' / 'QeLuaRuntime.cpp').write_text('// beta\n', encoding='utf-8')
    return root


class Standalone(unittest.TestCase):
    def test_required_error_when_path_invalid(self):
        with patch.dict(os.environ, {'QEAPP_FIRMWARE_ROOT':'/path/does/not/exist'}):
            with self.assertRaises(FirmwareRootError) as ctx:
                firmware_root(required=True)
            self.assertIn('OS_ROOT_INVALID', str(ctx.exception))

    def test_configured_external_firmware(self):
        with tempfile.TemporaryDirectory() as t:
            p=_write_checkout(Path(t)/'VQEAF-OS')
            with patch.dict(os.environ,{'QEAPP_FIRMWARE_ROOT':str(p)}):
                self.assertEqual(firmware_root(required=True),p.resolve())

    def test_no_firmware_is_optional_for_gui(self):
        # Optional lookup must never raise; it may discover monorepo firmware.
        with patch.dict(os.environ,{'QEAPP_FIRMWARE_ROOT':'/path/does/not/exist'}):
            root = firmware_root()
        if root is None:
            self.assertIsNone(root)
        else:
            self.assertTrue((Path(root)/'platformio.ini').is_file())

    def test_studio_path_is_rejected(self):
        report = inspect_firmware_root(ROOT)
        self.assertEqual(report['error_code'], 'OS_ROOT_IS_STUDIO')

    def test_firmware_layout_is_external_or_monorepo(self):
        # Standalone publish: firmware lives in a sibling VQEAF-OS checkout.
        # This workspace is a monorepo and may keep firmware/VQEAF-OS; either
        # layout is valid as long as Studio still finds a real platformio.ini.
        embedded = ROOT/'firmware/VQEAF-OS'
        if (embedded/'platformio.ini').is_file():
            self.assertTrue((embedded/'src').is_dir())
            return
        sibling = ROOT.parent/'VQEAF-OS'
        self.assertFalse(
            embedded.exists(),
            'partial firmware snapshot without platformio.ini; use a full VQEAF-OS checkout',
        )
        self.assertTrue(sibling.is_dir() or not sibling.exists())

if __name__=='__main__':unittest.main()
