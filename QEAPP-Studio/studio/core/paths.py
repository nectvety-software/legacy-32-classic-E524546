"""Per-user application paths for QEAPP Studio.

Storage mechanism mirrors LuaS30-IDE:

    %APPDATA%\\QEAPP-Studio\\{config,logs,cache,temp,backups}
    ~\\Documents\\QEAPP-Studio Projects\\<project folder>\\<structure>

Legacy locations (``QEAPPStudio`` app data, ``QEAPP Projects`` documents)
are migrated in place with a rename, falling back to merge-copy so user
data is never orphaned. Overrides: ``QEAPP_APPDATA``, ``QEAPP_DOCUMENTS``,
``QEAPP_PROJECTS``.
"""
from __future__ import annotations

import os
import shutil
from collections.abc import Sequence
from pathlib import Path

APP_FOLDER = 'QEAPP-Studio'
LEGACY_APP_FOLDERS = ('QEAPPStudio',)
PROJECTS_FOLDER = 'QEAPP-Studio Projects'
LEGACY_PROJECTS_FOLDERS = ('QEAPP Projects',)
USER_DIRS = ('config', 'logs', 'cache', 'temp', 'backups')

_INVALID_DIR_CHARS = frozenset('<>:"/\\|?*')
_RESERVED_NAMES = frozenset(
    {'CON', 'PRN', 'AUX', 'NUL'}
    | {f'COM{index}' for index in range(1, 10)}
    | {f'LPT{index}' for index in range(1, 10)}
)

__all__ = [
    'APP_FOLDER', 'LEGACY_APP_FOLDERS', 'PROJECTS_FOLDER', 'LEGACY_PROJECTS_FOLDERS',
    'USER_DIRS', 'app_data_root', 'config_dir', 'logs_dir', 'cache_dir', 'temp_dir',
    'backups_dir', 'ensure_user_dirs', 'migrate_config_file', 'resolve_config_file',
    'documents_root', 'projects_root', 'is_default_projects_root', 'project_dirname',
]


def _base_dirs() -> list[Path]:
    bases: list[Path] = []
    for key in ('APPDATA', 'LOCALAPPDATA', 'XDG_CONFIG_HOME'):
        value = os.environ.get(key)
        if value and value.strip():
            bases.append(Path(value).expanduser())
    if not bases:
        bases.append(Path.home() / '.config')
    return bases


def _resolve_folder(base: Path, current_name: str, legacy_names: Sequence[str], *, migrate: bool = True) -> Path:
    target = base / current_name
    if target.exists():
        return target
    for name in legacy_names:
        legacy = base / name
        if legacy.exists():
            if not migrate:
                return legacy
            try:
                os.rename(legacy, target)
            except OSError:
                return legacy  # keep old data instead of pointing at an empty folder
            return target
    return target


def app_data_root(*, create: bool = True, migrate: bool = True) -> Path:
    configured = os.environ.get('QEAPP_APPDATA')
    if configured and configured.strip():
        root = Path(configured.strip()).expanduser()
        if not root.exists():
            root = _resolve_folder(root.parent, root.name, LEGACY_APP_FOLDERS, migrate=migrate)
    else:
        root = _resolve_folder(_base_dirs()[0], APP_FOLDER, LEGACY_APP_FOLDERS, migrate=migrate)
    if create:
        root.mkdir(parents=True, exist_ok=True)
    return root


def _legacy_roots(root: Path) -> list[Path]:
    parent = root.parent
    out = []
    for name in LEGACY_APP_FOLDERS:
        candidate = parent / name
        if candidate.is_dir() and candidate != root:
            out.append(candidate)
    return out


def user_dir(name: str) -> Path:
    if name not in USER_DIRS:
        raise ValueError(f'Unknown application folder: {name}')
    path = app_data_root() / name
    path.mkdir(parents=True, exist_ok=True)
    return path


def config_dir() -> Path:
    return user_dir('config')


def logs_dir() -> Path:
    return user_dir('logs')


def cache_dir() -> Path:
    return user_dir('cache')


def temp_dir() -> Path:
    return user_dir('temp')


def backups_dir() -> Path:
    return user_dir('backups')


def ensure_user_dirs() -> None:
    root = app_data_root()
    for name in USER_DIRS:
        (root / name).mkdir(parents=True, exist_ok=True)


def migrate_config_file(filename: str) -> Path:
    """Return ``<app data>/config/<filename>``, pulling in any legacy copy."""
    target = config_dir() / filename
    if target.exists():
        return target
    root = app_data_root()
    candidates = [root / filename]
    candidates.extend(legacy / filename for legacy in _legacy_roots(root))
    for candidate in candidates:
        if candidate == target or not candidate.is_file():
            continue
        try:
            candidate.replace(target)
        except OSError:
            try:
                shutil.copy2(candidate, target)
            except OSError:
                pass
        break
    return target


def resolve_config_file(filename: str) -> Path:
    """Read-only lookup: existing location of a config file, legacy included."""
    root = app_data_root(create=False, migrate=False)
    target = root / 'config' / filename
    if target.exists():
        return target
    candidates = [root / filename]
    candidates.extend(legacy / filename for legacy in _legacy_roots(root))
    for candidate in candidates:
        if candidate != target and candidate.is_file():
            return candidate
    return target


def documents_root() -> Path:
    configured = os.environ.get('QEAPP_DOCUMENTS')
    if configured and configured.strip():
        return Path(configured.strip()).expanduser()
    if os.name == 'nt':
        try:
            import ctypes

            buffer = ctypes.create_unicode_buffer(260)
            result = ctypes.windll.shell32.SHGetFolderPathW(None, 5, None, 0, buffer)  # CSIDL_PERSONAL
            if result == 0 and buffer.value:
                return Path(buffer.value)
        except (AttributeError, OSError, ValueError):
            pass
    return Path.home() / 'Documents'


def _merge_move(source: Path, target: Path) -> None:
    for child in source.iterdir():
        destination = target / child.name
        if destination.exists():
            continue
        try:
            os.rename(child, destination)
        except OSError:
            try:
                if child.is_dir():
                    shutil.copytree(child, destination)
                else:
                    shutil.copy2(child, destination)
            except OSError:
                continue


def _migrate_projects_folder(root: Path) -> None:
    for name in LEGACY_PROJECTS_FOLDERS:
        legacy = documents_root() / name
        if not legacy.is_dir():
            continue
        if not root.exists():
            try:
                os.rename(legacy, root)
                continue
            except OSError:
                try:
                    root.mkdir(parents=True, exist_ok=True)
                except OSError:
                    continue
        if legacy.resolve() == root.resolve():
            continue
        _merge_move(legacy, root)


def projects_root(*, create: bool = True) -> Path:
    configured = os.environ.get('QEAPP_PROJECTS')
    if configured and configured.strip():
        root = Path(configured.strip()).expanduser()
    else:
        root = documents_root() / PROJECTS_FOLDER
        _migrate_projects_folder(root)
    if create and not root.exists():
        root.mkdir(parents=True, exist_ok=True)
    elif root.exists() and not root.is_dir():
        raise NotADirectoryError(str(root))
    return root


def is_default_projects_root(value: str) -> bool:
    """True when *value* points at the old or current default projects root."""
    text = str(value or '').strip()
    if not text:
        return False
    try:
        documents = documents_root()
        defaults = {documents / PROJECTS_FOLDER}
        defaults.update(documents / name for name in LEGACY_PROJECTS_FOLDERS)
        candidate = os.path.normcase(os.path.abspath(os.path.expanduser(text)))
        return candidate in {os.path.normcase(os.path.abspath(str(item))) for item in defaults}
    except (OSError, ValueError):
        return False


def project_dirname(name: str) -> str:
    """Validate a project display name and return its folder name."""
    text = str(name or '').strip()
    if not text:
        raise ValueError('Tên dự án không được để trống')
    if len(text) > 40:
        raise ValueError('Tên dự án tối đa 40 ký tự')
    if not text.isascii() or any(ord(char) < 32 or ord(char) > 126 for char in text):
        raise ValueError('Tên dự án phải là ký tự ASCII có thể in (1..40)')
    invalid = sorted({char for char in text if char in _INVALID_DIR_CHARS})
    if invalid:
        raise ValueError('Tên thư mục dự án không được chứa: ' + ' '.join(invalid))
    if text != text.rstrip(' .'):
        raise ValueError('Tên thư mục dự án không được kết thúc bằng dấu chấm hoặc khoảng trắng')
    if text.split('.')[0].upper() in _RESERVED_NAMES:
        raise ValueError(f'"{text}" là tên thư mục dành riêng của Windows')
    return text
