"""Offscreen Qt smoke for firmware Settings / Recovery dialogs."""
from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))


class FirmwareDialogQt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
        try:
            from PySide6.QtWidgets import QApplication
        except ImportError:
            raise unittest.SkipTest('PySide6 not installed: GUI has NOT been run here')
        cls.app = QApplication.instance() or QApplication([])

    def test_recovery_dialog_blocks_studio_path(self):
        from studio.core.errors import StudioError
        from studio.gui.firmware_settings import FirmwareRecoveryDialog

        dialog = FirmwareRecoveryDialog(None, error=StudioError('OS_ROOT_MISSING'), current=str(ROOT))
        dialog.path_edit.setText(str(ROOT))
        dialog._retry()
        self.assertIsNone(dialog.selected_path)
        self.assertIn('OS_ROOT', dialog.verdict.text() + dialog.error.code)

    def test_settings_rejects_studio_folder(self):
        from studio.gui.firmware_settings import FirmwareSettingsDialog

        dialog = FirmwareSettingsDialog(None, current=str(ROOT))
        dialog.path_edit.setText(str(ROOT))
        dialog._accept()
        self.assertIsNone(dialog.selected_path)

    def test_settings_accepts_real_firmware_tree(self):
        from studio.gui.firmware_settings import FirmwareSettingsDialog

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'VQEAF-OS'
            (root / 'tools').mkdir(parents=True)
            (root / 'platformio.ini').write_text('[env]\n', encoding='utf-8')
            (root / 'tools' / 'build_qeapp.py').write_text('#\n', encoding='utf-8')
            dialog = FirmwareSettingsDialog(None, current='')
            dialog.path_edit.setText(str(root))
            dialog._accept()
            self.assertEqual(dialog.selected_path, str(root))


if __name__ == '__main__':
    unittest.main()
