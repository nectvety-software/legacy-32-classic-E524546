"""Offscreen Qt smoke for NewProjectDialog (real Qt; skips without PySide6)."""
from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))


class NewProjectDialogQt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
        try:
            from PySide6.QtWidgets import QApplication
        except ImportError:
            raise unittest.SkipTest('PySide6 not installed: GUI has NOT been run here')
        cls.app = QApplication.instance() or QApplication([])

    def test_dialog_creates_standard_project(self):
        from studio.core.app_id import collect_app_ids
        from studio.gui.new_project_dialog import NewProjectDialog
        from PySide6.QtWidgets import QDialog

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dialog = NewProjectDialog(self.app.activeWindow(), projects_root=root)
            dialog.display_name.setText('Pocket Demo')
            # project_name auto-syncs until edited
            self.assertEqual(dialog.project_name.text(), 'Pocket_Demo')
            self.assertTrue(dialog.app_id.text().startswith('app_'))
            first_id = dialog.app_id.text()
            dialog._reroll_app_id()
            self.assertTrue(dialog.app_id.text().startswith('app_'))
            # Random button should usually change the id; uniqueness is the contract.
            dialog.app_id.setText('app_unique001')
            dialog._accept()
            self.assertIsNotNone(dialog.result_config)
            result = dialog.get_result()
            self.assertEqual(result.app_id, 'app_unique001')
            self.assertEqual(result.display_name, 'Pocket Demo')
            self.assertEqual(result.destination, root / 'Pocket_Demo')

            # Actually create via the same path window.py uses
            from studio.core.project_scaffold import create_standard_lua_project
            project = create_standard_lua_project(
                result.destination, result.app_id, result.display_name,
                project_name=result.project_name, used_app_ids=collect_app_ids(root),
            )
            self.assertTrue((project / 'qeapp.project.json').is_file())
            self.assertTrue((project / 'main.lua').is_file())
            self.assertTrue((project / 'assets' / 'icon.png').is_file())

    def test_dialog_rejects_duplicate_app_id(self):
        from studio.gui.new_project_dialog import NewProjectDialog
        from studio.core.project_scaffold import create_standard_lua_project

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            create_standard_lua_project(root / 'Exists', 'app_taken0001', 'Exists', project_name='Exists')
            dialog = NewProjectDialog(None, projects_root=root)
            dialog.project_name.setProperty('userEdited', True)
            dialog.project_name.setText('Second')
            dialog.display_name.setText('Second App')
            dialog.app_id.setText('app_taken0001')
            dialog._accept()
            self.assertIsNone(dialog.result_config)
            self.assertIn('tồn tại', dialog.status.text())


if __name__ == '__main__':
    unittest.main()
