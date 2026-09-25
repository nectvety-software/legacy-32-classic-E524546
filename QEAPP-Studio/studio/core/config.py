"""Small user-level UI configuration. Signing credentials are NEVER persisted.

Stored at ``%APPDATA%\\QEAPP-Studio\\config\\settings.json`` (legacy
``%APPDATA%\\QEAPPStudio\\settings.json`` is migrated automatically).
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import tempfile

from .paths import migrate_config_file

def default_settings_file() -> Path:
    return migrate_config_file('settings.json')

class StudioConfig:
    ALLOWED = ('recent_projects', 'firmware_root', 'last_open_folder', 'projects_root')

    def __init__(self, file: Path | None = None):
        if file is None:
            file = default_settings_file()
        self.file = Path(file)
        self.data = {'recent_projects': [], 'firmware_root': '', 'last_open_folder': '', 'projects_root': ''}
        if self.file.is_file():
            try:
                raw = json.loads(self.file.read_text(encoding='utf-8'))
                if isinstance(raw, dict):
                    for key in self.ALLOWED:
                        if key in raw and isinstance(raw[key], type(self.data[key])):
                            self.data[key] = raw[key]
            except (ValueError, UnicodeError, OSError):
                pass  # Invalid settings must not stop the editor opening.

    def update(self, **fields) -> None:
        if any(key not in self.ALLOWED for key in fields):
            raise ValueError('Unsupported preference (keys are never stored)')
        if 'recent_projects' in fields:
            items = fields['recent_projects']
            if not isinstance(items, list) or any(not isinstance(x, str) for x in items):
                raise ValueError('Recent projects must be string paths')
            fields['recent_projects'] = list(dict.fromkeys(items))[:8]
        for key in ('firmware_root', 'last_open_folder', 'projects_root'):
            if key in fields and not isinstance(fields[key], str):
                raise ValueError('Expected path string')
        self.data.update(fields)
        self.file.parent.mkdir(parents=True, exist_ok=True)
        handle, tmp_name = tempfile.mkstemp(prefix='.settings-', suffix='.json', dir=self.file.parent)
        try:
            with os.fdopen(handle, 'w', encoding='utf-8') as fd:
                fd.write(json.dumps(self.data, indent=2, ensure_ascii=False) + '\n')
                fd.flush()
                os.fsync(fd.fileno())
            os.replace(tmp_name, self.file)
        finally:
            Path(tmp_name).unlink(missing_ok=True)

    def remember(self, project: Path) -> None:
        real = str(Path(project).resolve())
        self.update(recent_projects=[real] + [p for p in self.data['recent_projects'] if p != real])
