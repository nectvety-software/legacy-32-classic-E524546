"""Runs only on machines with PySide6. Skip is visible (never count as PASS)."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

try:
    HAS_QT=importlib.util.find_spec('PySide6') is not None
except ValueError:
    HAS_QT=False

@unittest.skipUnless(HAS_QT, 'PySide6 not installed: GUI has NOT been run here')
class QtSmoke(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ['QT_QPA_PLATFORM']='offscreen'
        from PySide6.QtWidgets import QApplication
        cls.app = QApplication.instance() or QApplication([])

    def test_create_project_and_open_editor(self):
        from studio.gui.window import StudioWindow
        from tools.qstudio import init_project
        with tempfile.TemporaryDirectory() as d:
            project=Path(d)/'small'
            init_project('text',project,'small','Small')
            view=StudioWindow()
            view._set_project(project)
            view.open_document('content.txt')
            self.assertEqual(view.tabs.count(),1)
            self.assertIn('content.txt',view.editors)
            view.close()

    def test_initial_window(self):
        from studio.gui.window import StudioWindow
        view=StudioWindow()
        self.assertIn('QEAPP Studio',view.windowTitle())
        self.assertFalse(view.btn_stop.isEnabled())
        view.close()
