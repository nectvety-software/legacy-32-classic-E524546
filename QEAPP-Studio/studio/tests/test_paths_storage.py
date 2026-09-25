"""Storage mechanism tests: APPDATA config layout + Documents projects root.

Mirrors the LuaS30-IDE scheme: ``%APPDATA%\\QEAPP-Studio\\config\\settings.json``
and ``<Documents>\\QEAPP-Studio Projects\\<project folder>\\<structure>``.
"""
from __future__ import annotations

import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from studio.core import paths
from studio.core.config import StudioConfig
from studio.core.project_library import default_projects_root


class _EnvSandbox(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix='qeapp-storage-'))
        self.env = patch.dict(os.environ, {
            'QEAPP_APPDATA': str(self.root / 'QEAPP-Studio'),
            'QEAPP_DOCUMENTS': str(self.root / 'Documents'),
            'QEAPP_PROJECTS': '',
        })
        self.env.start()
        self.addCleanup(self.env.stop)
        self.addCleanup(shutil.rmtree, self.root, True)

    def _legacy_appdata(self) -> Path:
        legacy = self.root / 'QEAPPStudio'
        legacy.mkdir(parents=True)
        legacy.joinpath('settings.json').write_text(
            json.dumps({'recent_projects': ['kept'], 'firmware_root': ''}),
            encoding='utf-8',
        )
        legacy.joinpath('ai.json').write_text('{"provider": "ollama"}', encoding='utf-8')
        return legacy

    def _legacy_project(self) -> Path:
        project = self.root / 'Documents' / 'QEAPP Projects' / 'demo game'
        project.mkdir(parents=True)
        project.joinpath('qeapp.project.json').write_text('{"project_format": 1}', encoding='utf-8')
        project.joinpath('main.lua').write_text('print("hi")\n', encoding='utf-8')
        return project


class AppDataLayoutTests(_EnvSandbox):
    def test_legacy_appdata_folder_is_renamed_and_file_moved_into_config(self):
        legacy = self._legacy_appdata()
        config = StudioConfig()
        self.assertEqual(config.data['recent_projects'], ['kept'])
        self.assertEqual(config.file, self.root / 'QEAPP-Studio' / 'config' / 'settings.json')
        self.assertTrue(config.file.is_file())
        self.assertFalse(legacy.exists())

    def test_ensure_user_dirs_creates_reference_structure(self):
        paths.ensure_user_dirs()
        created = sorted(item.name for item in paths.app_data_root().iterdir())
        self.assertEqual(created, sorted(paths.USER_DIRS))

    def test_ai_settings_resolve_into_config_dir(self):
        self._legacy_appdata()
        from studio.core.ai_agent import _default_settings_file

        target = _default_settings_file()
        self.assertEqual(target, self.root / 'QEAPP-Studio' / 'config' / 'ai.json')
        self.assertTrue(target.is_file())

    def test_resolve_config_file_is_read_only(self):
        legacy = self._legacy_appdata()
        resolved = paths.resolve_config_file('settings.json')
        self.assertEqual(resolved, legacy / 'settings.json')
        self.assertTrue(legacy.exists())

    def test_repair_settings_path_matches_app(self):
        from studio.core import repair

        self._legacy_appdata()
        self.assertEqual(repair.settings_path(), self.root / 'QEAPPStudio' / 'settings.json')


class ProjectsRootTests(_EnvSandbox):
    def test_legacy_documents_folder_is_migrated(self):
        source = self._legacy_project()
        root = paths.projects_root()
        self.assertEqual(root, self.root / 'Documents' / 'QEAPP-Studio Projects')
        self.assertTrue(root.is_dir())
        self.assertTrue((root / 'demo game' / 'qeapp.project.json').is_file())
        self.assertTrue((root / 'demo game' / 'main.lua').is_file())
        self.assertFalse(source.parent.exists())

    def test_default_projects_root_delegates_to_paths(self):
        self.assertEqual(default_projects_root(), paths.projects_root())

    def test_projects_root_override_env_wins(self):
        custom = self.root / 'custom-projects'
        with patch.dict(os.environ, {'QEAPP_PROJECTS': str(custom)}):
            self.assertEqual(paths.projects_root(), custom)
            self.assertTrue(custom.is_dir())

    def test_existing_root_still_absorbs_leftover_legacy_projects(self):
        root = self.root / 'Documents' / 'QEAPP-Studio Projects'
        (root / 'existing').mkdir(parents=True)
        leftover = self.root / 'Documents' / 'QEAPP Projects' / 'leftover'
        leftover.mkdir(parents=True)
        leftover.joinpath('qeapp.project.json').write_text('{"project_format": 1}', encoding='utf-8')
        resolved = paths.projects_root()
        self.assertEqual(resolved, root)
        self.assertTrue((root / 'leftover' / 'qeapp.project.json').is_file())
        self.assertTrue((root / 'existing').is_dir())


class ProjectDirnameTests(unittest.TestCase):
    def test_accepts_printable_ascii_folder_names(self):
        self.assertEqual(paths.project_dirname('  My Game 1  '), 'My Game 1')
        self.assertEqual(paths.project_dirname('snake_v2'), 'snake_v2')
        self.assertEqual(paths.project_dirname('trailing '), 'trailing')

    def test_rejects_windows_invalid_names(self):
        for value in ('a/b', 'a\\b', 'a:b', 'a<b', 'a>b', 'a|b', 'a?b', 'a*b', 'a"b',
                      'CON', 'com1', 'LPT9', 'trailing.', '', '   '):
            with self.subTest(value=value), self.assertRaises(ValueError):
                paths.project_dirname(value)

    def test_rejects_non_ascii_and_oversized_names(self):
        with self.assertRaises(ValueError):
            paths.project_dirname('dự án')
        with self.assertRaises(ValueError):
            paths.project_dirname('x' * 41)


class DefaultRootDetectionTests(_EnvSandbox):
    def test_detects_legacy_and_current_default_only(self):
        documents = self.root / 'Documents'
        self.assertTrue(paths.is_default_projects_root(str(documents / 'QEAPP Projects')))
        self.assertTrue(paths.is_default_projects_root(str(documents / 'QEAPP-Studio Projects')))
        self.assertFalse(paths.is_default_projects_root(str(documents / 'My Own')))
        self.assertFalse(paths.is_default_projects_root(''))
        self.assertFalse(paths.is_default_projects_root('   '))


if __name__ == '__main__':
    unittest.main()
