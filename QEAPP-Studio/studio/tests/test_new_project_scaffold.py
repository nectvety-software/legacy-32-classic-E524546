"""Tests for unique App IDs and Standard Lua Application scaffold (PRJ-01..08)."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from studio.core.app_id import collect_app_ids, is_valid_app_id, suggested_app_id
from studio.core.project_scaffold import (
    STANDARD_LAYOUT, ScaffoldError, create_standard_lua_project,
)


class AppIdTests(unittest.TestCase):
    def test_valid_id_pattern(self):
        self.assertTrue(is_valid_app_id('app_a1b2c3d4'))
        self.assertTrue(is_valid_app_id('my_app-1'))
        self.assertFalse(is_valid_app_id('My App'))
        self.assertFalse(is_valid_app_id(''))
        self.assertFalse(is_valid_app_id('x' * 25))

    def test_unique_ids_across_projects(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            first = create_standard_lua_project(root / 'One', suggested_app_id(root), 'One App')
            used = collect_app_ids(root)
            second_id = suggested_app_id(root, exclude=first)
            self.assertNotIn(second_id, used)
            second = create_standard_lua_project(root / 'Two', second_id, 'Two App', used_app_ids=used)
            ids = collect_app_ids(root)
            self.assertEqual(ids, {json.loads((first / 'qeapp.project.json').read_text())['id'],
                                   json.loads((second / 'qeapp.project.json').read_text())['id']})

    def test_random_button_ids_do_not_collide(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            seen = {suggested_app_id(root) for _ in range(20)}
            self.assertEqual(len(seen), 20)


class ScaffoldTests(unittest.TestCase):
    def _create(self, root: Path, app_id='app_test0001', name='My Application', folder='MyApplication'):
        return create_standard_lua_project(root / folder, app_id, name, project_name=folder)

    def test_full_standard_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            project = self._create(Path(tmp))
            for rel in STANDARD_LAYOUT:
                self.assertTrue((project / rel).is_file(), rel)
            payload = json.loads((project / 'qeapp.project.json').read_text(encoding='utf-8'))
            self.assertEqual(payload['type'], 'lua')
            self.assertEqual(payload['content'], 'main.lua')
            self.assertEqual(payload['icon'], 'assets/icon.png')
            self.assertEqual(payload['version'], '1.0.0')
            self.assertEqual(
                set(payload),
                {'project_format', 'id', 'name', 'version', 'type', 'content', 'icon'},
            )
            lua = (project / 'main.lua').read_text(encoding='utf-8')
            self.assertIn('function on_key', lua)
            self.assertIn('function on_draw', lua)
            self.assertIn('function on_update', lua)

    def test_cli_validate_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            project = self._create(Path(tmp), folder='CliApp')
            sys.path.insert(0, str(ROOT / 'tools'))
            from qstudio import validate
            obj, assets = validate(project)
            self.assertEqual(obj['type'], 'lua')
            self.assertIn('content', assets)
            self.assertIn('icon', assets)

    def test_qstudio_init_lua_and_lua_standard_equivalent(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sys.path.insert(0, str(ROOT / 'tools'))
            from qstudio import init_project, validate
            init_project('lua-standard', root / 'A', 'app_aaaaaaaa', 'App A')
            init_project('lua', root / 'B', 'app_bbbbbbbb', 'App B')
            for name in ('A', 'B'):
                project = root / name
                validate(project)
                for rel in STANDARD_LAYOUT:
                    self.assertTrue((project / rel).is_file(), f'{name}/{rel}')
            self.assertEqual(
                set(json.loads((root / 'A' / 'qeapp.project.json').read_text())),
                set(json.loads((root / 'B' / 'qeapp.project.json').read_text())),
            )

    def test_reject_duplicate_app_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self._create(root, app_id='app_same0001', folder='One')
            used = collect_app_ids(root)
            with self.assertRaises(ScaffoldError):
                create_standard_lua_project(
                    root / 'Two', 'app_same0001', 'Two App',
                    project_name='Two', used_app_ids=used,
                )
            self.assertFalse((root / 'Two').exists())

    def test_reject_existing_destination(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / 'Taken'
            target.mkdir()
            with self.assertRaises(ScaffoldError):
                create_standard_lua_project(target, 'app_taken001', 'Taken', project_name='Taken')
            self.assertEqual(list(target.iterdir()), [])

    def test_reject_invalid_id_and_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(ScaffoldError):
                create_standard_lua_project(root / 'X', 'BAD ID', 'Name', project_name='X')
            with self.assertRaises(ScaffoldError):
                create_standard_lua_project(root / 'Y', 'app_ok00001', 'Bad/Name', project_name='Y')
            with self.assertRaises(ScaffoldError):
                create_standard_lua_project(root / 'Z', 'app_ok00002', 'Name', project_name='Z/x')
            self.assertFalse((root / 'X').exists())
            self.assertFalse((root / 'Y').exists())
            self.assertFalse((root / 'Z').exists())

    def test_icon_is_png_32(self):
        import struct
        with tempfile.TemporaryDirectory() as tmp:
            project = self._create(Path(tmp), folder='IconApp')
            icon = (project / 'assets' / 'icon.png').read_bytes()
            self.assertEqual(icon[:8], b'\x89PNG\r\n\x1a\n')
            width, height = struct.unpack('>II', icon[16:24])
            self.assertEqual((width, height), (32, 32))

    def test_replay_has_start_press(self):
        with tempfile.TemporaryDirectory() as tmp:
            project = self._create(Path(tmp), folder='ReplayApp')
            events = json.loads((project / 'tests' / 'input_replay.json').read_text(encoding='utf-8'))
            downs = {(e['frame'], e['key'], e['down']) for e in events}
            self.assertIn((1, 'start', True), downs)
            self.assertIn((2, 'start', False), downs)


class GuiDialogSourceTests(unittest.TestCase):
    def test_dialog_source_has_random_unique_fields(self):
        source = (ROOT / 'studio' / 'gui' / 'new_project_dialog.py').read_text(encoding='utf-8')
        self.assertIn('suggested_app_id', source)
        self.assertIn('Project name', source)
        self.assertIn('Display name', source)
        self.assertIn('App ID', source)
        self.assertIn('Random', source)
        self.assertIn('lua-standard', source)


if __name__ == '__main__':
    unittest.main()
