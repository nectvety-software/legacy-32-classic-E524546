from __future__ import annotations

from collections.abc import Iterable, Mapping
from dataclasses import dataclass
from datetime import datetime
import json
import math
import os
from pathlib import Path
import re
import stat

__all__ = [
    'MAX_BYTES', 'MAX_DEPTH', 'MAX_DIRECTORY_ENTRIES', 'MAX_FILES', 'MAX_METADATA_BYTES',
    'MAX_RECENT_PATHS', 'MAX_SCAN_BYTES', 'MAX_SCAN_DEPTH', 'MAX_SCAN_DIRECTORIES',
    'MAX_SCAN_FILES', 'MAX_SCAN_PROJECTS', 'PROJECT_FILE', 'PROJECT_FILENAME',
    'ProjectLibrary', 'ProjectRecord', 'default_projects_root',
]

PROJECT_FILENAME = 'qeapp.project.json'
PROJECT_FILE = PROJECT_FILENAME
MAX_METADATA_BYTES = 256 * 1024
MAX_SCAN_DEPTH = 8
MAX_SCAN_DIRECTORIES = 512
MAX_SCAN_FILES = 5000
MAX_SCAN_BYTES = 128 * 1024 * 1024
MAX_SCAN_PROJECTS = 256
MAX_DIRECTORY_ENTRIES = 10_000
MAX_RECENT_PATHS = 64
MAX_DEPTH = MAX_SCAN_DEPTH
MAX_FILES = MAX_SCAN_FILES
MAX_BYTES = MAX_SCAN_BYTES

_SKIP_NAMES = frozenset({
    '.git', '.hg', '.svn', '.venv', 'venv', 'virtualenv', 'env',
    '__pycache__', 'build', 'dist', 'out', 'target', 'artifacts', 'node_modules',
    'coverage', 'cache', 'caches', 'reports', 'release', 'output', 'keys', 'key',
    'private', 'private_keys', 'secrets', 'credentials', 'certificates', 'certs',
    'signing_keys',
})
_KEY_SUFFIXES = frozenset({'.pem', '.key', '.p12', '.pfx', '.jks', '.keystore'})
_REPARSE_POINT = 0x400
_HIDDEN_ATTRIBUTE = 0x2
_KEY_WORD = re.compile(r'(^|[-_. ])(key|keys|private|secret|credential|credentials)([-_. ]|$)')
_SKIP_WORD = re.compile(r'(^|[-_. ])(build|dist|out|target|artifact|artifacts|venv|virtualenv|node_modules)([-_. ]|$)')


def default_projects_root(*, create: bool = True) -> Path:
    from .paths import projects_root

    return projects_root(create=create)


def _timestamp(value) -> float:
    if isinstance(value, datetime):
        try:
            result = float(value.timestamp())
        except (OverflowError, OSError, ValueError):
            return 0.0
        return result if math.isfinite(result) else 0.0
    try:
        result = float(value)
    except (TypeError, ValueError, OverflowError):
        return 0.0
    return result if math.isfinite(result) else 0.0


def _text(value, fallback: str = '', limit: int = 160) -> str:
    if not isinstance(value, str):
        return fallback
    value = value.replace('\x00', '').strip()
    if not value:
        return fallback
    return value[:limit]


def _absolute(path: Path | str) -> Path:
    return Path(os.path.realpath(os.path.abspath(os.fspath(path))))


def _path_key(path: Path | str) -> str:
    return os.path.normcase(os.path.abspath(os.fspath(path)))


def _is_reparse(path: Path, info: os.stat_result | None = None) -> bool:
    try:
        if path.is_symlink():
            return True
        info = info or path.lstat()
    except OSError:
        return True
    return bool(getattr(info, 'st_file_attributes', 0) & _REPARSE_POINT)


def _is_hidden(path: Path, info: os.stat_result | None = None) -> bool:
    if path.name.startswith('.'):
        return True
    try:
        info = info or path.lstat()
    except OSError:
        return True
    return bool(getattr(info, 'st_file_attributes', 0) & _HIDDEN_ATTRIBUTE)


def _forbidden_name(name: str) -> bool:
    lower = name.casefold()
    if lower in _SKIP_NAMES or lower.startswith('.'):
        return True
    if lower.endswith(('_key', '_keys', '-key', '-keys')) or lower.startswith(('key-', 'keys-', 'key_', 'keys_')):
        return True
    return bool(_KEY_WORD.search(lower) or _SKIP_WORD.search(lower))


def _key_file(name: str) -> bool:
    lower = name.casefold()
    return Path(lower).suffix in _KEY_SUFFIXES or lower in {
        'id_rsa', 'id_ed25519', 'id_ecdsa', 'id_dsa', 'private_key', 'private.pem',
    }


def _safe_existing_directory(path: Path | str) -> Path | None:
    try:
        candidate = _absolute(path)
        absolute = _absolute(candidate)
        current = Path(absolute.anchor) if absolute.anchor else Path()
        parts = absolute.parts[1:] if absolute.anchor else absolute.parts
        for part in parts:
            if part.startswith('.'):
                return None
            current = current / part
            try:
                info = current.lstat()
            except FileNotFoundError:
                return None
            except OSError:
                return None
            if _is_reparse(current, info) or not stat.S_ISDIR(info.st_mode):
                return None
        if not absolute.is_dir():
            return None
        return absolute
    except (OSError, TypeError, ValueError):
        return None


def _has_metadata(project: Path) -> bool:
    try:
        metadata = project / PROJECT_FILENAME
        info = metadata.lstat()
        return stat.S_ISREG(info.st_mode) and not _is_reparse(metadata, info)
    except OSError:
        return False


def _safe_metadata(project: Path) -> Path | None:
    if _forbidden_name(project.name) or not _safe_existing_directory(project):
        return None
    metadata = project / PROJECT_FILENAME
    try:
        info = metadata.lstat()
        if not stat.S_ISREG(info.st_mode) or _is_reparse(metadata, info):
            return None
        if info.st_size > MAX_METADATA_BYTES:
            return None
        raw = metadata.read_bytes()
        if len(raw) > MAX_METADATA_BYTES or b'\x00' in raw:
            return None
        value = json.loads(raw.decode('utf-8'))
    except (OSError, UnicodeError, ValueError, TypeError, RecursionError):
        return None
    if not isinstance(value, dict):
        return None
    return metadata


def _read_metadata(project: Path) -> dict | None:
    metadata = _safe_metadata(project)
    if metadata is None:
        return None
    try:
        value = json.loads(metadata.read_text(encoding='utf-8'))
    except (OSError, UnicodeError, ValueError, TypeError, RecursionError):
        return None
    return value if isinstance(value, dict) else None


def _regular(info: os.stat_result) -> bool:
    return stat.S_ISREG(info.st_mode)


def _directory(info: os.stat_result) -> bool:
    return stat.S_ISDIR(info.st_mode)


def _skip_entry(name: str, info: os.stat_result | None = None, path: Path | None = None) -> bool:
    if name.startswith('.') or _forbidden_name(name):
        return True
    if path is not None and info is not None and _is_hidden(path, info):
        return True
    return False


def _bounded_children(folder: Path, limit: int) -> tuple[list[os.DirEntry], bool]:
    children: list[os.DirEntry] = []
    truncated = False
    try:
        with os.scandir(folder) as entries:
            for entry in entries:
                children.append(entry)
                if len(children) >= limit:
                    truncated = True
                    break
    except (OSError, ValueError):
        return [], True
    children.sort(key=lambda item: item.name.casefold())
    return children, truncated


@dataclass(frozen=True, slots=True, init=False)
class ProjectRecord:
    path: Path
    name: str
    id: str
    type: str
    version: str
    modified: float
    file_count: int
    size_bytes: int
    build_status: str
    recent: bool
    managed: bool
    truncated: bool

    def __init__(
        self,
        path: Path | str | None = None,
        name: str = '',
        id: str | None = None,
        type: str = '',
        version: str = '',
        modified: float | datetime | None = None,
        file_count: int = 0,
        size_bytes: int | None = None,
        build_status: str = 'not-built',
        recent: bool = False,
        managed: bool = True,
        truncated: bool = False,
        *,
        project_id: str | None = None,
        project_type: str | None = None,
        project_path: Path | str | None = None,
        project_name: str | None = None,
        project_version: str | None = None,
        folder: Path | str | None = None,
        modified_at: float | datetime | None = None,
        modified_time: float | datetime | None = None,
        last_modified: float | datetime | None = None,
        timestamp: float | datetime | None = None,
        total_size: int | None = None,
        size: int | None = None,
        file_size: int | None = None,
        is_recent: bool | None = None,
        is_managed: bool | None = None,
        status: str | None = None,
        build: str | bool | None = None,
        built: bool | None = None,
    ):
        selected_path = path if path is not None else project_path
        if selected_path is None:
            selected_path = folder
        if project_name is not None:
            name = project_name
        if project_version is not None:
            version = project_version
        if selected_path is None:
            selected_path = Path('.')
        try:
            selected_path = Path(os.path.abspath(os.fspath(selected_path)))
        except (OSError, TypeError, ValueError):
            selected_path = Path('.')
        selected_id = project_id if project_id is not None else id
        selected_type = project_type if project_type is not None else type
        selected_modified = timestamp if timestamp is not None else modified_at
        if selected_modified is None:
            selected_modified = modified_time if modified_time is not None else last_modified
        if selected_modified is None:
            selected_modified = modified
        selected_size = total_size if total_size is not None else size
        if selected_size is None:
            selected_size = file_size if file_size is not None else size_bytes
        if selected_size is None:
            selected_size = 0
        selected_status = status if status is not None else build
        if selected_status is None and built is not None:
            selected_status = 'built' if built else 'not-built'
        if selected_status is None:
            selected_status = build_status
        try:
            count = max(0, int(file_count))
        except (TypeError, ValueError, OverflowError):
            count = 0
        try:
            byte_count = max(0, int(selected_size))
        except (TypeError, ValueError, OverflowError):
            byte_count = 0
        normalized_status = _text(selected_status, 'not-built', 32).casefold().replace(' ', '-')
        object.__setattr__(self, 'path', selected_path)
        object.__setattr__(self, 'name', _text(name, selected_path.name, 120) or selected_path.name)
        object.__setattr__(self, 'id', _text(selected_id, selected_path.name, 80) or selected_path.name)
        object.__setattr__(self, 'type', _text(selected_type, 'unknown', 40))
        object.__setattr__(self, 'version', _text(version, '', 40))
        object.__setattr__(self, 'modified', _timestamp(selected_modified))
        object.__setattr__(self, 'file_count', count)
        object.__setattr__(self, 'size_bytes', byte_count)
        object.__setattr__(self, 'build_status', normalized_status or 'not-built')
        object.__setattr__(self, 'recent', bool(is_recent if is_recent is not None else recent))
        object.__setattr__(self, 'managed', bool(is_managed if is_managed is not None else managed))
        object.__setattr__(self, 'truncated', bool(truncated))

    @property
    def project_id(self) -> str:
        return self.id

    @property
    def project_type(self) -> str:
        return self.type

    @property
    def project_path(self) -> Path:
        return self.path

    @property
    def project_name(self) -> str:
        return self.name

    @property
    def project_version(self) -> str:
        return self.version

    @property
    def modified_at(self) -> datetime | None:
        if not self.modified:
            return None
        try:
            return datetime.fromtimestamp(self.modified)
        except (OSError, OverflowError, ValueError):
            return None

    @property
    def last_modified(self) -> float:
        return self.modified

    @property
    def modified_time(self) -> float:
        return self.modified

    @property
    def timestamp(self) -> float:
        return self.modified

    @property
    def files(self) -> int:
        return self.file_count

    @property
    def size(self) -> int:
        return self.size_bytes

    @property
    def total_size(self) -> int:
        return self.size_bytes

    @property
    def file_size(self) -> int:
        return self.size_bytes

    @property
    def status(self) -> str:
        return self.build_status

    @property
    def build_status_label(self) -> str:
        return self.build_status.replace('-', ' ').title()

    @property
    def is_built(self) -> bool:
        return self.build_status == 'built'

    @property
    def has_build(self) -> bool:
        return self.is_built

    @property
    def is_recent(self) -> bool:
        return self.recent

    @property
    def is_managed(self) -> bool:
        return self.managed

    @property
    def is_recent_project(self) -> bool:
        return self.recent

    def __fspath__(self) -> str:
        return str(self.path)

    def as_dict(self) -> dict:
        return {
            'path': self.path,
            'name': self.name,
            'id': self.id,
            'type': self.type,
            'version': self.version,
            'modified': self.modified,
            'file_count': self.file_count,
            'size_bytes': self.size_bytes,
            'build_status': self.build_status,
            'recent': self.recent,
            'managed': self.managed,
            'truncated': self.truncated,
        }


@dataclass
class _Budget:
    directories: int = 0
    files: int = 0
    bytes: int = 0
    projects: int = 0
    truncated: bool = False

    def directory(self, limit: int) -> bool:
        if self.directories >= limit:
            self.truncated = True
            return False
        self.directories += 1
        return True

    def file(self, size: int, file_limit: int, byte_limit: int) -> bool:
        if self.files >= file_limit:
            self.truncated = True
            return False
        remaining = byte_limit - self.bytes
        if remaining <= 0:
            self.truncated = True
            return False
        self.files += 1
        self.bytes += max(0, min(size, remaining))
        return size <= remaining

    def project(self, limit: int) -> bool:
        if self.projects >= limit:
            self.truncated = True
            return False
        self.projects += 1
        return True


class ProjectLibrary:
    def __init__(
        self,
        root: Path | str | None = None,
        recent_paths: Iterable[Path | str] | None = None,
        *,
        managed_root: Path | str | None = None,
        projects_root: Path | str | None = None,
        recent: Iterable[Path | str] | None = None,
        recent_projects: Iterable[Path | str] | None = None,
        max_depth: int = MAX_SCAN_DEPTH,
        max_directories: int = MAX_SCAN_DIRECTORIES,
        max_files: int = MAX_SCAN_FILES,
        max_bytes: int = MAX_SCAN_BYTES,
        max_projects: int = MAX_SCAN_PROJECTS,
        max_recent: int = MAX_RECENT_PATHS,
    ):
        roots = [value for value in (root, managed_root, projects_root) if value is not None]
        if len(roots) > 1:
            raise ValueError('Specify only one projects root')
        selected_root = roots[0] if roots else None
        self.root = (Path(selected_root).expanduser() if selected_root is not None else default_projects_root()).resolve()
        self.max_depth = self._limit(max_depth, 32, 'max_depth', 0)
        self.max_directories = self._limit(max_directories, 100_000, 'max_directories', 1)
        self.max_files = self._limit(max_files, 1_000_000, 'max_files', 1)
        self.max_bytes = self._limit(max_bytes, 1_000_000_000_000, 'max_bytes', 1)
        self.max_projects = self._limit(max_projects, 10_000, 'max_projects', 1)
        self.max_recent = self._limit(max_recent, 1000, 'max_recent', 1)
        recent_values = [value for value in (recent_paths, recent, recent_projects) if value is not None]
        if len(recent_values) > 1:
            raise ValueError('Specify recent projects only once')
        selected_recent = recent_values[0] if recent_values else None
        self._recent_paths: list[Path] = []
        self.set_recent_paths(selected_recent or ())

    @staticmethod
    def _limit(value: int, ceiling: int, name: str, minimum: int) -> int:
        try:
            result = int(value)
        except (TypeError, ValueError, OverflowError) as exc:
            raise ValueError(f'{name} must be an integer') from exc
        if result < minimum or result > ceiling:
            raise ValueError(f'{name} is outside the safe range')
        return result

    @property
    def projects_root(self) -> Path:
        return self.root

    @property
    def recent_paths(self) -> tuple[Path, ...]:
        return tuple(self._recent_paths)

    def set_recent_paths(self, paths: Iterable[Path | str] | None) -> None:
        if paths is None:
            values: Iterable[Path | str] = ()
        elif isinstance(paths, (str, Path)):
            values = (paths,)
        else:
            values = paths
        result: list[Path] = []
        seen: set[str] = set()
        try:
            iterator = iter(values)
        except TypeError:
            iterator = iter(())
        for value in iterator:
            try:
                path = Path(os.path.abspath(os.fspath(value)))
            except (OSError, TypeError, ValueError):
                continue
            key = _path_key(path)
            if key not in seen:
                seen.add(key)
                result.append(path)
            if len(result) >= self.max_recent:
                break
        self._recent_paths = result

    def add_recent(self, path: Path | str) -> None:
        self.set_recent_paths((path, *self._recent_paths))

    def remember(self, path: Path | str) -> None:
        self.add_recent(path)

    def remove_recent(self, path: Path | str) -> None:
        try:
            key = _path_key(path)
        except (TypeError, ValueError, OSError):
            return
        self._recent_paths = [item for item in self._recent_paths if _path_key(item) != key]

    def _discover(self, root: Path, budget: _Budget) -> list[Path]:
        root = _safe_existing_directory(root) or root
        if not _safe_existing_directory(root):
            return []
        if _forbidden_name(root.name):
            return []
        if _has_metadata(root):
            return [root] if budget.project(self.max_projects) else []
        found: list[Path] = []
        stack: list[tuple[Path, int]] = [(root, 0)]
        while stack:
            if budget.projects >= self.max_projects:
                budget.truncated = True
                break
            current, level = stack.pop()
            if level > self.max_depth or not budget.directory(self.max_directories):
                continue
            children, children_truncated = _bounded_children(current, min(MAX_DIRECTORY_ENTRIES, self.max_files + self.max_directories + 1))
            if children_truncated:
                budget.truncated = True
            directories: list[Path] = []
            for entry in children:
                name = entry.name
                try:
                    info = entry.stat(follow_symlinks=False)
                except OSError:
                    continue
                path = Path(entry.path)
                if _skip_entry(name, info, path):
                    continue
                if _is_reparse(path, info):
                    continue
                if not _directory(info):
                    continue
                child = _absolute(path)
                if _has_metadata(child):
                    if budget.project(self.max_projects):
                        found.append(child)
                    continue
                if level < self.max_depth:
                    directories.append(child)
            for child in reversed(directories):
                stack.append((child, level + 1))
        return found

    def _project_path(self, value: Path | str) -> Path | None:
        try:
            path = _absolute(value)
        except (OSError, TypeError, ValueError):
            return None
        try:
            info = path.lstat()
            if stat.S_ISREG(info.st_mode) and path.name.casefold() == PROJECT_FILENAME.casefold():
                path = path.parent
        except OSError:
            return None
        safe = _safe_existing_directory(path)
        if safe is None or _forbidden_name(safe.name) or not _has_metadata(safe):
            return None
        return safe

    def _within_root(self, path: Path) -> bool:
        try:
            root = _path_key(self.root)
            candidate = _path_key(path)
            return os.path.commonpath((root, candidate)) == root
        except (OSError, ValueError):
            return False

    def _stats(self, project: Path, budget: _Budget) -> tuple[int, int, float, bool]:
        try:
            root_info = project.lstat()
            root_modified = float(root_info.st_mtime) if stat.S_ISDIR(root_info.st_mode) else 0.0
        except OSError:
            root_modified = 0.0
        modified = 0.0
        count = 0
        size = 0
        truncated = budget.truncated
        if not _safe_existing_directory(project):
            return count, size, root_modified, True
        stack: list[tuple[Path, int]] = [(project, 0)]
        while stack:
            if not budget.directory(self.max_directories):
                truncated = True
                break
            current, level = stack.pop()
            children, children_truncated = _bounded_children(current, min(MAX_DIRECTORY_ENTRIES, self.max_files + self.max_directories + 1))
            if children_truncated:
                truncated = True
            directories: list[Path] = []
            for entry in children:
                name = entry.name
                try:
                    info = entry.stat(follow_symlinks=False)
                except OSError:
                    continue
                path = Path(entry.path)
                if _skip_entry(name, info, path):
                    continue
                if _is_reparse(path, info):
                    continue
                if _directory(info):
                    if level < self.max_depth and not _has_metadata(path):
                        directories.append(path)
                    continue
                if not _regular(info) or _key_file(name):
                    continue
                try:
                    file_size = max(0, int(info.st_size))
                    file_modified = float(info.st_mtime)
                except (TypeError, ValueError, OverflowError):
                    continue
                if not math.isfinite(file_modified):
                    continue
                modified = max(modified, file_modified)
                if not budget.file(file_size, self.max_files, self.max_bytes):
                    truncated = True
                    break
                count += 1
                size += file_size
            for child in reversed(directories):
                stack.append((child, level + 1))
        if not modified:
            modified = root_modified
        return count, size, modified, truncated

    def _artifact_status(self, project: Path) -> str:
        for folder_name in ('dist', 'build', 'artifacts'):
            folder = project / folder_name
            if _safe_existing_directory(folder) is None:
                continue
            children, _ = _bounded_children(folder, min(256, MAX_DIRECTORY_ENTRIES))
            for entry in children:
                try:
                    info = entry.stat(follow_symlinks=False)
                except OSError:
                    continue
                if not _is_reparse(Path(entry.path), info) and _regular(info) and entry.name.casefold().endswith('.qeapp'):
                    return 'built'
        for report_name in ('build-status.json', 'build_status.json'):
            report = project / report_name
            try:
                info = report.lstat()
                if not _regular(info) or _is_reparse(report, info) or info.st_size > MAX_METADATA_BYTES:
                    continue
                value = json.loads(report.read_text(encoding='utf-8'))
            except (OSError, UnicodeError, ValueError, TypeError, RecursionError):
                continue
            if isinstance(value, Mapping):
                value = value.get('status', value.get('build_status'))
            if isinstance(value, str):
                status = value.casefold().replace(' ', '-')
                if status in {'built', 'success', 'passed', 'pass', 'ready', 'ok', 'complete'}:
                    return 'built'
                if status in {'stale', 'outdated'}:
                    return 'stale'
                if status in {'failed', 'error', 'failure'}:
                    return 'failed'
        return 'not-built'

    def scan(self) -> list[ProjectRecord]:
        budget = _Budget()
        recent_projects: list[Path] = []
        recent_keys: set[str] = set()
        for value in self._recent_paths:
            project = self._project_path(value)
            if project is None:
                continue
            key = _path_key(project)
            if key not in recent_keys:
                recent_keys.add(key)
                recent_projects.append(project)
        managed_projects = self._discover(self.root, budget)
        candidates: list[Path] = []
        seen: set[str] = set()
        for project in (*recent_projects, *managed_projects):
            key = _path_key(project)
            if key in seen:
                continue
            seen.add(key)
            candidates.append(project)
            if len(candidates) >= self.max_projects:
                budget.truncated = True
                break
        records: list[ProjectRecord] = []
        for project in candidates:
            metadata = _read_metadata(project)
            if metadata is None:
                continue
            key = _path_key(project)
            count, size, modified, truncated = self._stats(project, budget)
            name = _text(metadata.get('name'), project.name, 120) or project.name
            project_id = _text(metadata.get('id', metadata.get('project_id')), project.name, 80) or project.name
            project_type = _text(metadata.get('type', metadata.get('project_type')), 'unknown', 40)
            version = _text(metadata.get('version'), '', 40)
            records.append(ProjectRecord(
                path=project,
                name=name,
                id=project_id,
                type=project_type,
                version=version,
                modified=modified,
                file_count=count,
                size_bytes=size,
                build_status=self._artifact_status(project),
                recent=key in recent_keys,
                managed=self._within_root(project),
                truncated=truncated or budget.truncated,
            ))
        recent_rank = {_path_key(path): index for index, path in enumerate(self._recent_paths)}
        records.sort(key=lambda item: (0 if item.recent else 1,
                                       recent_rank.get(_path_key(item.path), 1_000_000) if item.recent else 0,
                                       -item.modified, item.name.casefold()))
        return records

    def scan_projects(self) -> list[ProjectRecord]:
        return self.scan()

    def list_projects(self) -> list[ProjectRecord]:
        return self.scan()

    def scan_recent(self) -> list[ProjectRecord]:
        return self.recent()

    def scan_all(self) -> list[ProjectRecord]:
        return self.all_projects()

    @property
    def projects(self) -> list[ProjectRecord]:
        return self.scan()

    @property
    def recent_projects(self) -> list[ProjectRecord]:
        return self.recent()

    def refresh(self) -> list[ProjectRecord]:
        return self.scan()

    def records(self) -> list[ProjectRecord]:
        return self.scan()

    def all_projects(self) -> list[ProjectRecord]:
        return self.scan()

    def recent(self) -> list[ProjectRecord]:
        return [record for record in self.scan() if record.recent]
