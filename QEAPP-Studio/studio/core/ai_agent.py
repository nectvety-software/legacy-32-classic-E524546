from __future__ import annotations

from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass, field, replace
import fnmatch
import ipaddress
import json
import os
from pathlib import Path, PurePosixPath
import re
import socket
import stat
import tempfile
import threading
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlsplit, urlunsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener

from .workspace import ALLOWED_EXTENSIONS, Workspace

MAX_CONTEXT_FILES = 80
MAX_CONTEXT_FILE_BYTES = 16 * 1024
MAX_CONTEXT_BYTES = 96 * 1024
MAX_CONTEXT_DEPTH = 12
MAX_CONTEXT_SCAN_ENTRIES = 5000
MAX_EXTRA_CONTEXT_BYTES = 24 * 1024
MAX_APPEND_CONTEXT_CHARS = 12 * 1024
MAX_TOOL_OUTPUT_BYTES = 32 * 1024
MAX_TOOL_FILES = 240
MAX_TOOL_MATCHES = 120
MAX_TOOL_PATTERN_CHARS = 240
MAX_REQUEST_BYTES = 128 * 1024
MAX_RESPONSE_BYTES = 256 * 1024
MAX_MESSAGE_CHARS = 16 * 1024
MAX_MESSAGES = 24
MAX_TOOL_CALLS = 16
MAX_TOOL_BLOCKS = 16
DEFAULT_TIMEOUT = 30.0
MIN_TIMEOUT = 0.01
MAX_TIMEOUT = 300.0
DEFAULT_MAX_TOKENS = 2048
MAX_TOKENS = 8192

PROVIDER_DEFAULTS = {
    'openai': ('gpt-4o-mini', 'https://api.openai.com/v1'),
    'ollama': ('llama3.2', 'http://127.0.0.1:11434'),
    'anthropic': ('claude-3-5-sonnet-20241022', 'https://api.anthropic.com'),
    'gemini': ('gemini-1.5-flash', 'https://generativelanguage.googleapis.com'),
}

PROVIDER_ENV_KEYS = {
    'openai': ('OPENAI_API_KEY', 'AI_API_KEY'),
    'ollama': ('OLLAMA_API_KEY', 'AI_API_KEY'),
    'anthropic': ('ANTHROPIC_API_KEY', 'AI_API_KEY'),
    'gemini': ('GEMINI_API_KEY', 'GOOGLE_API_KEY', 'AI_API_KEY'),
}

EXCLUDED_DIRECTORY_NAMES = frozenset({
    '.git', '.hg', '.svn', '.venv', 'venv', '__pycache__', 'node_modules',
    'build', 'dist', 'out', 'target', 'bin', 'obj', 'coverage', 'cache',
    'caches', '.cache', 'logs', 'tmp', 'temp', 'vendor', 'generated',
})

SECRET_FILE_NAMES = frozenset({
    'id_rsa', 'id_dsa', 'id_ecdsa', 'id_ed25519', 'credentials', 'credentials.json',
    'secrets', 'secrets.json', 'secret.json', 'private_key', 'private_key.pem',
    'token', 'token.json', 'password', 'passwords', 'passwd', '.env', '.env.local',
})

SECRET_EXTENSIONS = frozenset({'.pem', '.key', '.p12', '.pfx', '.crt', '.cer', '.jks', '.keystore'})
SECRET_MARKERS = ('secret', 'credential', 'password', 'passwd', 'private', 'token', 'api_key', 'apikey', 'api-key', 'access_token', 'refresh_token')
ALLOWED_TEXT_EXTENSIONS = frozenset(ALLOWED_EXTENSIONS)

SYSTEM_INSTRUCTIONS = (
    'You are the QEAPP Studio project assistant. Be concise and technically precise. '
    'Only use the supplied project context. You may inspect project text with read, grep, and glob, '
    'and inspect reported problems. Never request shell execution, credentials, signing keys, '
    'or automatic file writes. To inspect the project, return one JSON object in a fenced block '
    'labelled qeapp-tool using an allowed action and an arguments object, for example '
    '{"action":"read","arguments":{"path":"main.lua"}}.'
)

SAFE_TOOL_ACTIONS = frozenset({'read', 'grep', 'glob', 'problems'})


class AIAgentError(RuntimeError):
    pass


class AIConfigurationError(AIAgentError, ValueError):
    pass


class AICancelled(AIAgentError):
    pass


class AITimeoutError(AIAgentError, TimeoutError):
    pass


class AIConnectionError(AIAgentError):
    pass


class AIHTTPError(AIAgentError):
    pass


class AIResponseError(AIAgentError):
    pass


class ToolError(ValueError):
    pass


class ToolParseError(ValueError):
    pass


def _safe_text(value: Any, limit: int, field: str) -> str:
    if not isinstance(value, str):
        raise AIConfigurationError(f'{field} must be text')
    if len(value) > limit or any(ord(char) < 32 and char not in '\t' for char in value):
        raise AIConfigurationError(f'{field} contains unsupported characters')
    return value.strip()


def _clip_utf8(value: str, limit: int) -> tuple[str, bool]:
    if limit <= 0:
        return '', bool(value)
    raw = value.encode('utf-8', errors='replace')
    if len(raw) <= limit:
        return value, False
    return raw[:limit].decode('utf-8', errors='ignore'), True


def _json_bytes(value: Any, limit: int) -> tuple[bytes, bool]:
    raw = json.dumps(value, ensure_ascii=False, separators=(',', ':'), default=str).encode('utf-8')
    return raw[:limit], len(raw) > limit


def _redact(value: Any, *secrets: str | None) -> str:
    text = str(value)
    for secret in secrets:
        if secret:
            text = text.replace(secret, '[REDACTED]')
    return text[:2000]


def _redact_value(value: Any, *secrets: str | None) -> Any:
    if isinstance(value, str):
        text = value
        for secret in secrets:
            if secret:
                text = text.replace(secret, '[REDACTED]')
        return text
    if isinstance(value, Mapping):
        return {str(key): _redact_value(item, *secrets) for key, item in value.items()}
    if isinstance(value, list):
        return [_redact_value(item, *secrets) for item in value]
    if isinstance(value, tuple):
        return tuple(_redact_value(item, *secrets) for item in value)
    return value


def normalize_provider(provider: str) -> str:
    if not isinstance(provider, str):
        raise AIConfigurationError('provider must be text')
    value = provider.strip().casefold().replace('_', '-')
    aliases = {
        'openai': 'openai',
        'openai-compatible': 'openai',
        'openai-compat': 'openai',
        'compatible': 'openai',
        'ollama': 'ollama',
        'anthropic': 'anthropic',
        'claude': 'anthropic',
        'gemini': 'gemini',
        'google': 'gemini',
        'google-gemini': 'gemini',
    }
    if value not in aliases:
        raise AIConfigurationError('Unsupported AI provider')
    return aliases[value]


def validate_base_url(value: str) -> str:
    if not isinstance(value, str):
        raise AIConfigurationError('base URL must be text')
    value = value.strip()
    if not value or len(value) > 2048 or any(ord(char) < 32 for char in value):
        raise AIConfigurationError('Invalid base URL')
    try:
        parsed = urlsplit(value)
    except ValueError as exc:
        raise AIConfigurationError('Invalid base URL') from exc
    if parsed.scheme.casefold() not in ('http', 'https') or not parsed.netloc:
        raise AIConfigurationError('Base URL must use http or https')
    if parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise AIConfigurationError('Base URL cannot contain credentials or query data')
    try:
        parsed.port
    except ValueError as exc:
        raise AIConfigurationError('Invalid base URL port') from exc
    if not parsed.hostname:
        raise AIConfigurationError('Base URL has no host')
    path = parsed.path.rstrip('/')
    return urlunsplit((parsed.scheme.casefold(), parsed.netloc, path, '', ''))


def _is_loopback_host(host: str) -> bool:
    value = host.casefold().strip('[]')
    if value == 'localhost':
        return True
    try:
        return ipaddress.ip_address(value).is_loopback
    except ValueError:
        return False


def _validate_model(model: str) -> str:
    value = _safe_text(model, 200, 'model')
    if not value:
        raise AIConfigurationError('model must not be empty')
    return value


def _default_settings_file() -> Path:
    configured = os.getenv('QEAPP_STUDIO_CONFIG_DIR')
    if configured:
        return Path(configured).expanduser() / 'ai.json'
    from .paths import migrate_config_file

    return migrate_config_file('ai.json')


@dataclass(frozen=True)
class AISettings:
    provider: str = 'openai'
    model: str = ''
    base_url: str = ''

    def __post_init__(self) -> None:
        provider = normalize_provider(self.provider)
        default_model, default_url = PROVIDER_DEFAULTS[provider]
        model = _validate_model(self.model or default_model)
        base_url = validate_base_url(self.base_url or default_url)
        object.__setattr__(self, 'provider', provider)
        object.__setattr__(self, 'model', model)
        object.__setattr__(self, 'base_url', base_url)

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> 'AISettings':
        if not isinstance(value, Mapping):
            raise AIConfigurationError('AI settings must be an object')
        return cls(
            provider=str(value.get('provider', 'openai')),
            model=str(value.get('model', '')),
            base_url=str(value.get('base_url', '')),
        )

    def to_dict(self) -> dict[str, str]:
        return {
            'provider': self.provider,
            'model': self.model,
            'base_url': self.base_url,
        }

    def updated(self, **values: str) -> 'AISettings':
        if any(key not in {'provider', 'model', 'base_url'} for key in values):
            raise AIConfigurationError('Only provider, model and base URL are stored')
        selected_provider = normalize_provider(values.get('provider', self.provider))
        changed_provider = selected_provider != self.provider
        default_model, default_url = PROVIDER_DEFAULTS[selected_provider]
        return replace(
            self,
            provider=selected_provider,
            model=values.get('model', default_model if changed_provider else self.model),
            base_url=values.get('base_url', default_url if changed_provider else self.base_url),
        )


class AISettingsStore:
    ALLOWED = frozenset({'provider', 'model', 'base_url'})

    def __init__(self, file: Path | str | None = None):
        self.file = Path(file).expanduser() if file is not None else _default_settings_file()
        self._settings = self._load()

    def _load(self) -> AISettings:
        if not self.file.is_file():
            return AISettings()
        try:
            raw = json.loads(self.file.read_text(encoding='utf-8'))
            if not isinstance(raw, Mapping):
                return AISettings()
            filtered = {key: raw[key] for key in self.ALLOWED if key in raw and isinstance(raw[key], str)}
            return AISettings.from_mapping(filtered)
        except (OSError, UnicodeError, ValueError, TypeError, AIConfigurationError):
            return AISettings()

    @property
    def settings(self) -> AISettings:
        return self._settings

    @property
    def data(self) -> dict[str, str]:
        return self._settings.to_dict()

    def load(self) -> AISettings:
        self._settings = self._load()
        return self._settings

    def save(self, settings: AISettings | Mapping[str, Any]) -> AISettings:
        value = settings if isinstance(settings, AISettings) else AISettings.from_mapping(settings)
        self.file.parent.mkdir(parents=True, exist_ok=True)
        handle, temp_name = tempfile.mkstemp(prefix='.ai-settings-', suffix='.json', dir=self.file.parent)
        try:
            try:
                os.chmod(temp_name, stat.S_IRUSR | stat.S_IWUSR)
            except OSError:
                pass
            with os.fdopen(handle, 'w', encoding='utf-8') as stream:
                json.dump(value.to_dict(), stream, ensure_ascii=False, indent=2)
                stream.write('\n')
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temp_name, self.file)
        finally:
            Path(temp_name).unlink(missing_ok=True)
        self._settings = value
        return value

    def update(self, **values: str) -> AISettings:
        if any(key not in self.ALLOWED for key in values):
            raise AIConfigurationError('API keys and unknown settings are not stored')
        return self.save(self._settings.updated(**values))


@dataclass(frozen=True)
class ContextFile:
    path: str
    text: str
    size: int
    truncated: bool


@dataclass(frozen=True)
class ProjectContext:
    text: str
    files: tuple[ContextFile, ...] = ()
    total_bytes: int = 0
    truncated: bool = False

    @property
    def content(self) -> str:
        return self.text

    @property
    def file_count(self) -> int:
        return len(self.files)

    def __str__(self) -> str:
        return self.text


def _workspace_root(workspace: Any) -> Path:
    if workspace is None:
        raise ToolError('No project is selected')
    if isinstance(workspace, Workspace):
        candidate = workspace.root
    elif isinstance(workspace, (str, bytes, os.PathLike)):
        candidate = workspace
    else:
        candidate = getattr(workspace, 'root', workspace)
    try:
        root = Path(candidate).resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise ToolError('Project root is unavailable') from exc
    if not root.is_dir():
        raise ToolError('Project root is not a directory')
    metadata = root / 'qeapp.project.json'
    if metadata.is_symlink() or not metadata.is_file():
        raise ToolError('Project must contain a regular qeapp.project.json')
    return root


def _coerce_workspace(workspace: Any) -> Workspace | None:
    if workspace is None:
        return None
    if isinstance(workspace, Workspace):
        return workspace
    root = _workspace_root(workspace)
    return Workspace(root)


def _is_secret_path(parts: Sequence[str], *, allow_metadata: bool = False) -> bool:
    if allow_metadata and tuple(parts) == ('qeapp.project.json',):
        return False
    for part in parts:
        lower = part.casefold()
        if part.startswith('.') or lower in EXCLUDED_DIRECTORY_NAMES or lower in SECRET_FILE_NAMES:
            return True
        if Path(part).suffix.casefold() in SECRET_EXTENSIONS:
            return True
        if any(marker in lower for marker in SECRET_MARKERS):
            return True
    return False


def _validate_relative_path(value: str, *, label: str = 'path') -> tuple[str, ...]:
    if not isinstance(value, str) or not value or len(value) > 512:
        raise ToolError(f'Invalid {label}')
    if '\\' in value or '\x00' in value or any(ord(char) < 32 for char in value):
        raise ToolError(f'Invalid {label}')
    if value.startswith('/') or value.startswith('~'):
        raise ToolError(f'{label} must be relative')
    raw_parts = value.split('/')
    if any(part in {'.', '..'} for part in raw_parts) or (raw_parts and len(raw_parts[0]) == 2 and raw_parts[0][1] == ':'):
        raise ToolError(f'Invalid {label}')
    path = PurePosixPath(value)
    parts = path.parts
    if not parts or any(part in {'.', '..'} for part in parts):
        raise ToolError(f'Invalid {label}')
    if _is_secret_path(parts, allow_metadata=True):
        raise ToolError(f'{label} is excluded from project access')
    if path.suffix.casefold() not in ALLOWED_TEXT_EXTENSIONS and tuple(parts) != ('qeapp.project.json',):
        raise ToolError(f'{label} is not an allowed text file')
    return parts


def _regular_file(path: Path) -> bool:
    try:
        return path.is_file() and not path.is_symlink() and stat.S_ISREG(path.stat().st_mode)
    except OSError:
        return False


def _read_text_file(path: Path, limit: int) -> tuple[str, int, bool]:
    if path.is_symlink() or not path.is_file():
        raise ToolError('Project file is unavailable')
    try:
        with path.open('rb') as stream:
            data = stream.read(limit + 1)
    except OSError as exc:
        raise ToolError('Unable to read project file') from exc
    original_size = len(data)
    truncated = original_size > limit
    if truncated:
        data = data[:limit]
    if b'\x00' in data:
        raise ToolError('Binary project files are not available to tools')
    try:
        return data.decode('utf-8'), original_size, truncated
    except UnicodeDecodeError as exc:
        raise ToolError('Project files must be UTF-8 text') from exc


def _walk_project_files(root: Path, maximum: int) -> list[tuple[str, Path]]:
    result: list[tuple[str, Path]] = []
    scanned = 0

    def visit(folder: Path, relative: tuple[str, ...], depth: int) -> None:
        nonlocal scanned
        if depth > MAX_CONTEXT_DEPTH or len(result) >= maximum or scanned >= MAX_CONTEXT_SCAN_ENTRIES:
            return
        try:
            with os.scandir(folder) as iterator:
                entries = sorted(iterator, key=lambda entry: entry.name.casefold())
        except OSError:
            return
        for entry in entries:
            if scanned >= MAX_CONTEXT_SCAN_ENTRIES or len(result) >= maximum:
                return
            scanned += 1
            name = entry.name
            if name.startswith('.'):
                continue
            if entry.is_symlink():
                continue
            child = relative + (name,)
            if _is_secret_path(child, allow_metadata=True):
                continue
            try:
                if entry.is_dir(follow_symlinks=False):
                    if name.casefold() not in EXCLUDED_DIRECTORY_NAMES:
                        visit(Path(entry.path), child, depth + 1)
                elif entry.is_file(follow_symlinks=False):
                    if Path(name).suffix.casefold() in ALLOWED_TEXT_EXTENSIONS:
                        result.append(('/'.join(child), Path(entry.path)))
            except OSError:
                continue

    metadata = root / 'qeapp.project.json'
    if _regular_file(metadata):
        result.append(('qeapp.project.json', metadata))
    visit(root, (), 0)
    unique: dict[str, Path] = {}
    for rel, path in result:
        unique.setdefault(rel, path)
    ordered = sorted(unique.items(), key=lambda item: (item[0] != 'qeapp.project.json', item[0].casefold()))
    return ordered[:maximum]


def iter_allowed_project_files(workspace: Any, maximum: int = MAX_CONTEXT_FILES) -> tuple[tuple[str, Path], ...]:
    if not isinstance(maximum, int) or not 1 <= maximum <= MAX_TOOL_FILES:
        raise ToolError('Invalid project file limit')
    root = _workspace_root(workspace)
    return tuple(_walk_project_files(root, maximum))


def read_allowed_project_file(workspace: Any, relative: str, *, limit: int = MAX_CONTEXT_FILE_BYTES) -> str:
    parts = _validate_relative_path(relative)
    root = _workspace_root(workspace)
    path = root.joinpath(*parts)
    cursor = root
    for part in parts:
        cursor = cursor / part
        if cursor.is_symlink():
            raise ToolError('Symlinks are not available to project tools')
    try:
        path.resolve(strict=True).relative_to(root)
    except (OSError, ValueError) as exc:
        raise ToolError('Project path escapes the project root') from exc
    if not _regular_file(path):
        raise ToolError('Project file is unavailable')
    text, _, _ = _read_text_file(path, max(1, min(limit, MAX_CONTEXT_FILE_BYTES)))
    return text


class ProjectContextBuilder:
    def __init__(
        self,
        workspace: Any = None,
        *,
        max_files: int = MAX_CONTEXT_FILES,
        max_file_bytes: int = MAX_CONTEXT_FILE_BYTES,
        max_context_bytes: int = MAX_CONTEXT_BYTES,
    ):
        if not isinstance(max_files, int) or not 1 <= max_files <= MAX_CONTEXT_FILES:
            raise ValueError('Invalid context file limit')
        if not isinstance(max_file_bytes, int) or not 1 <= max_file_bytes <= MAX_CONTEXT_FILE_BYTES:
            raise ValueError('Invalid context file size limit')
        if not isinstance(max_context_bytes, int) or not 128 <= max_context_bytes <= MAX_CONTEXT_BYTES:
            raise ValueError('Invalid context size limit')
        self.max_files = max_files
        self.max_file_bytes = max_file_bytes
        self.max_context_bytes = max_context_bytes
        self.workspace = _coerce_workspace(workspace) if workspace is not None else None
        self._extra: list[tuple[str, str]] = []
        self._extra_bytes = 0

    def set_project(self, workspace: Any | None) -> None:
        self.workspace = _coerce_workspace(workspace) if workspace is not None else None

    def append_context(self, text: str, label: str = 'integration', *, source: str | None = None) -> None:
        if source is not None:
            label = source
        if not isinstance(text, str):
            raise ValueError('Context must be text')
        clean = text.replace('\x00', '')
        clipped, _ = _clip_utf8(clean, MAX_APPEND_CONTEXT_CHARS)
        if not clipped:
            return
        safe_label = _safe_text(label, 80, 'context label').replace('\n', ' ') or 'integration'
        remaining = MAX_EXTRA_CONTEXT_BYTES - self._extra_bytes
        if remaining <= 0:
            return
        clipped, _ = _clip_utf8(clipped, remaining)
        if clipped:
            self._extra.append((safe_label, clipped))
            self._extra_bytes += len(clipped.encode('utf-8'))

    def clear_context(self) -> None:
        self._extra.clear()
        self._extra_bytes = 0

    def build_report(self, workspace: Any | None = None) -> ProjectContext:
        selected = self.workspace if workspace is None else _coerce_workspace(workspace)
        chunks: list[str] = []
        files: list[ContextFile] = []
        used = 0
        truncated = False

        def add(value: str) -> bool:
            nonlocal used
            raw = value.encode('utf-8')
            remaining = self.max_context_bytes - used
            if remaining <= 0:
                return False
            if len(raw) > remaining:
                value = raw[:remaining].decode('utf-8', errors='ignore')
                raw = value.encode('utf-8')
            chunks.append(value)
            used += len(raw)
            return len(raw) <= remaining

        if selected is not None:
            root = _workspace_root(selected)
            project_name, _ = _clip_utf8(root.name, 64)
            add(f'Project: {project_name}\n')
            listed = iter_allowed_project_files(selected, min(self.max_files + 1, MAX_TOOL_FILES))
            truncated = len(listed) > self.max_files
            listed = listed[:self.max_files]
            for relative, path in listed:
                header = f'\n--- {relative} ---\n'
                header_bytes = len(header.encode('utf-8'))
                if used + header_bytes >= self.max_context_bytes:
                    truncated = True
                    break
                try:
                    text, size, file_truncated = _read_text_file(path, self.max_file_bytes)
                except ToolError:
                    if relative == 'qeapp.project.json':
                        text, size, file_truncated = '[metadata unavailable]', 0, True
                    else:
                        truncated = True
                        continue
                body, body_truncated = _clip_utf8(text, max(1, self.max_context_bytes - used - header_bytes))
                chunk = header + body
                before = used
                add(chunk)
                files.append(ContextFile(relative, body, size, file_truncated or body_truncated or used - before < len(chunk.encode('utf-8'))))
                truncated = truncated or file_truncated or body_truncated or used >= self.max_context_bytes
                if used >= self.max_context_bytes:
                    break
        else:
            add('Project: none\n')

        for label, text in self._extra:
            header = f'\n--- {label} context ---\n'
            if used + len(header.encode('utf-8')) >= self.max_context_bytes:
                truncated = True
                break
            body, body_truncated = _clip_utf8(text, max(1, self.max_context_bytes - used - len(header.encode('utf-8'))))
            add(header + body)
            truncated = truncated or body_truncated
            if used >= self.max_context_bytes:
                break
        if truncated and used < self.max_context_bytes:
            add('\n[context truncated]\n')
        return ProjectContext(''.join(chunks), tuple(files), used, truncated)

    def build(self, workspace: Any | None = None) -> str:
        return self.build_report(workspace).text


def build_project_context(workspace: Any = None, **limits: int) -> str:
    return ProjectContextBuilder(workspace, **limits).build()


def build_context(workspace: Any = None, **limits: int) -> str:
    return build_project_context(workspace, **limits)


@dataclass(frozen=True)
class ToolCall:
    action: str
    arguments: dict[str, Any] = field(default_factory=dict)
    raw: str = ''

    def __post_init__(self) -> None:
        if not isinstance(self.action, str) or not self.action.strip():
            raise ToolParseError('Tool action is missing')
        if not isinstance(self.arguments, Mapping):
            raise ToolParseError('Tool arguments must be an object')
        object.__setattr__(self, 'action', self.action.strip().casefold().replace('-', '_'))
        object.__setattr__(self, 'arguments', dict(self.arguments))

    def to_dict(self) -> dict[str, Any]:
        return {'action': self.action, 'arguments': dict(self.arguments)}

    def __getitem__(self, key: str) -> Any:
        if key == 'action':
            return self.action
        if key in {'arguments', 'args', 'params'}:
            return self.arguments
        return self.arguments[key]

    def get(self, key: str, default: Any = None) -> Any:
        try:
            return self[key]
        except KeyError:
            return default


_FENCE_RE = re.compile(r'```(?:qeapp-tool|qeapp_tool)[ \t]*(?:\r?\n)?(.*?)(?:\r?\n)?```', re.IGNORECASE | re.DOTALL)


def _tool_action(value: Any) -> str:
    if not isinstance(value, str):
        raise ToolParseError('Tool action must be text')
    action = value.strip().casefold().replace('-', '_')
    aliases = {
        'read_file': 'read',
        'file_read': 'read',
        'search': 'grep',
        'grep_text': 'grep',
        'list_files': 'glob',
        'glob_files': 'glob',
        'list_problems': 'problems',
        'diagnostics': 'problems',
    }
    return aliases.get(action, action)


def _calls_from_payload(payload: Any) -> list[ToolCall]:
    values = payload if isinstance(payload, list) else [payload]
    if len(values) > MAX_TOOL_CALLS:
        raise ToolParseError('Too many tool calls')
    calls: list[ToolCall] = []
    for value in values:
        if not isinstance(value, Mapping):
            raise ToolParseError('Each tool call must be an object')
        action_value = value.get('action', value.get('tool', value.get('name', value.get('type'))))
        action = _tool_action(action_value)
        if 'arguments' in value:
            arguments = value.get('arguments')
        elif 'args' in value:
            arguments = value.get('args')
        elif 'params' in value:
            arguments = value.get('params')
        else:
            arguments = {key: item for key, item in value.items() if key not in {'action', 'tool', 'name', 'type'}}
        if not isinstance(arguments, Mapping):
            raise ToolParseError('Tool arguments must be an object')
        calls.append(ToolCall(action, dict(arguments), json.dumps(value, ensure_ascii=False, separators=(',', ':'))))
    return calls


def parse_tool_blocks(text: str) -> tuple[list[ToolCall], list[str]]:
    if not isinstance(text, str):
        return [], ['Tool response must be text']
    if len(text) > MAX_CONTEXT_BYTES * 2:
        return [], ['Tool response is too large']
    calls: list[ToolCall] = []
    errors: list[str] = []
    for match in _FENCE_RE.finditer(text):
        if len(calls) + len(errors) >= MAX_TOOL_BLOCKS:
            errors.append('Too many tool blocks')
            break
        block = match.group(1).strip()
        if not block or len(block) > MAX_CONTEXT_FILE_BYTES:
            errors.append('Tool block is empty or too large')
            continue
        try:
            payload = json.loads(block)
        except (TypeError, ValueError):
            errors.append('Tool block contains invalid JSON')
            continue
        try:
            calls.extend(_calls_from_payload(payload))
        except ToolParseError as exc:
            errors.append(str(exc))
    if len(calls) > MAX_TOOL_CALLS:
        return [], ['Too many tool calls']
    return calls, errors


def parse_tool_calls(text: str) -> list[ToolCall]:
    calls, errors = parse_tool_blocks(text)
    if errors:
        raise ToolParseError(errors[0])
    return calls


def parse_qeapp_tool_json(text: str) -> list[ToolCall]:
    return parse_tool_calls(text)


parse_tool_call = parse_tool_calls


class QeappToolParser:
    parse = staticmethod(parse_tool_calls)
    parse_blocks = staticmethod(parse_tool_blocks)


ToolParser = QeappToolParser


@dataclass(frozen=True)
class ToolResult:
    action: str
    ok: bool
    data: Any = None
    error: str = ''

    def to_dict(self) -> dict[str, Any]:
        return {'action': self.action, 'ok': self.ok, 'data': self.data, 'error': self.error}

    def as_text(self) -> str:
        if not self.ok:
            return f'{self.action} error: {self.error}'
        try:
            data = json.dumps(self.data, ensure_ascii=False, separators=(',', ':'), default=str)
        except (TypeError, ValueError):
            data = str(self.data)
        return f'{self.action}: {data}'[:MAX_TOOL_OUTPUT_BYTES]


class SafeToolExecutor:
    def __init__(self, workspace: Any = None, problems_callback: Callable[[], Any] | None = None):
        self.workspace = workspace
        self.problems_callback = problems_callback

    def set_project(self, workspace: Any | None) -> None:
        self.workspace = workspace

    def set_problems_callback(self, callback: Callable[[], Any] | None) -> None:
        if callback is not None and not callable(callback):
            raise TypeError('Problems callback must be callable')
        self.problems_callback = callback

    def _result_error(self, action: str, error: Exception | str) -> ToolResult:
        text = _redact(error if isinstance(error, str) else str(error))
        return ToolResult(action, False, None, text)

    def _path_matches(self, relative: str, pattern: str) -> bool:
        try:
            pattern_parts = PurePosixPath(pattern).parts
        except ValueError:
            return False
        if any(part in {'.', '..'} or part.startswith('/') for part in pattern_parts):
            return False
        for part in pattern_parts:
            if not any(char in part for char in '*?['):
                if part.startswith('.') or part.casefold() in EXCLUDED_DIRECTORY_NAMES or part.casefold() in SECRET_FILE_NAMES:
                    return False
        if fnmatch.fnmatchcase(relative, pattern) or PurePosixPath(relative).match(pattern):
            return True
        if pattern.startswith('**/'):
            return fnmatch.fnmatchcase(relative, pattern[3:]) or PurePosixPath(relative).match(pattern[3:])
        return False

    def execute(self, call: ToolCall | Mapping[str, Any] | str, arguments: Mapping[str, Any] | None = None) -> ToolResult:
        action = 'tool'
        try:
            if isinstance(call, ToolCall):
                action = call.action
                call_arguments = call.arguments
            elif isinstance(call, Mapping):
                action = _tool_action(call.get('action', call.get('tool', call.get('name', call.get('type')))))
                if 'arguments' in call:
                    call_arguments = call.get('arguments')
                elif 'args' in call:
                    call_arguments = call.get('args')
                elif 'params' in call:
                    call_arguments = call.get('params')
                else:
                    call_arguments = {key: item for key, item in call.items() if key not in {'action', 'tool', 'name', 'type'}}
            else:
                action = _tool_action(call)
                call_arguments = {}
            if arguments is not None:
                if not isinstance(arguments, Mapping):
                    raise ToolError('Tool arguments must be an object')
                call_arguments = arguments
            if not isinstance(call_arguments, Mapping):
                raise ToolError('Tool arguments must be an object')
            action = _tool_action(action)
            if action not in SAFE_TOOL_ACTIONS:
                return self._result_error(action, 'Tool action is not allowed')
            if action == 'read':
                relative = call_arguments.get('path', call_arguments.get('file'))
                if not isinstance(relative, str):
                    raise ToolError('read requires path')
                return ToolResult(action, True, read_allowed_project_file(self.workspace, relative))
            if action == 'glob':
                pattern = call_arguments.get('pattern', call_arguments.get('glob', '*'))
                if not isinstance(pattern, str) or not pattern or len(pattern) > MAX_TOOL_PATTERN_CHARS:
                    raise ToolError('glob requires a short pattern')
                if any(ord(char) < 32 for char in pattern) or '\\' in pattern or pattern.startswith('/'):
                    raise ToolError('Invalid glob pattern')
                listed = iter_allowed_project_files(self.workspace, MAX_TOOL_FILES)
                paths = [relative for relative, _ in listed if self._path_matches(relative, pattern)]
                return ToolResult(action, True, paths[:MAX_TOOL_FILES])
            if action == 'grep':
                pattern = call_arguments.get('pattern', call_arguments.get('query', call_arguments.get('text')))
                if not isinstance(pattern, str) or not pattern or len(pattern) > MAX_TOOL_PATTERN_CHARS:
                    raise ToolError('grep requires a short pattern')
                if any(ord(char) < 32 for char in pattern):
                    raise ToolError('Invalid grep pattern')
                if call_arguments.get('regex') or call_arguments.get('regexp'):
                    return self._result_error(action, 'Regular expressions are disabled')
                case_sensitive = bool(call_arguments.get('case_sensitive', False))
                needle = pattern if case_sensitive else pattern.casefold()
                include = call_arguments.get('include', call_arguments.get('path'))
                if include is not None and not isinstance(include, str):
                    raise ToolError('Invalid include pattern')
                listed = iter_allowed_project_files(self.workspace, MAX_TOOL_FILES)
                matches: list[dict[str, Any]] = []
                scanned = 0
                for relative, _ in listed:
                    if include and not self._path_matches(relative, include):
                        continue
                    try:
                        text = read_allowed_project_file(self.workspace, relative)
                    except ToolError:
                        continue
                    scanned += len(text.encode('utf-8'))
                    for number, line in enumerate(text.splitlines(), 1):
                        haystack = line if case_sensitive else line.casefold()
                        position = haystack.find(needle)
                        if position >= 0:
                            matches.append({'path': relative, 'line': number, 'column': position + 1, 'excerpt': line[:240]})
                            if len(matches) >= MAX_TOOL_MATCHES or scanned >= MAX_CONTEXT_BYTES:
                                return ToolResult(action, True, {'matches': matches, 'truncated': True})
                return ToolResult(action, True, {'matches': matches, 'truncated': False})
            callback = self.problems_callback
            if callback is None:
                return ToolResult(action, True, [])
            value = callback()
            if isinstance(value, (str, bytes)):
                value = value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value
                value, _ = _clip_utf8(value, MAX_TOOL_OUTPUT_BYTES)
            else:
                try:
                    encoded = json.dumps(value, ensure_ascii=False, separators=(',', ':'), default=str)
                except (TypeError, ValueError):
                    encoded = str(value)
                if len(encoded.encode('utf-8')) > MAX_TOOL_OUTPUT_BYTES:
                    encoded, _ = _clip_utf8(encoded, MAX_TOOL_OUTPUT_BYTES)
                try:
                    value = json.loads(encoded)
                except (TypeError, ValueError):
                    value = encoded
            return ToolResult(action, True, value)
        except AICancelled:
            raise
        except (ToolError, OSError, UnicodeError, TypeError, ValueError) as exc:
            action_name = action if isinstance(action, str) else 'tool'
            return self._result_error(action_name, exc)
        except Exception as exc:
            action_name = action if isinstance(action, str) else 'tool'
            return self._result_error(action_name, exc)


@dataclass(frozen=True)
class AgentResponse:
    text: str
    tool_results: tuple[ToolResult, ...] = ()
    parse_errors: tuple[str, ...] = ()


class _NoRedirectHandler(HTTPRedirectHandler):
    def redirect_request(self, req: Request, fp: Any, code: int, msg: str, headers: Any, newurl: str) -> None:
        return None


class AIAgent:
    def __init__(
        self,
        settings: AISettings | Mapping[str, Any] | Path | str | None = None,
        *,
        config: AISettings | Mapping[str, Any] | None = None,
        settings_file: Path | str | None = None,
        provider: str | None = None,
        model: str | None = None,
        base_url: str | None = None,
        timeout: float = DEFAULT_TIMEOUT,
        max_response_bytes: int = MAX_RESPONSE_BYTES,
        transport: Callable[..., Any] | None = None,
    ):
        if config is not None:
            if settings is not None:
                raise TypeError('Use settings or config, not both')
            settings = config
        if isinstance(settings, (str, Path)):
            self.settings_store = AISettingsStore(settings)
            current = self.settings_store.settings
        elif isinstance(settings, AISettings):
            self.settings_store = AISettingsStore(settings_file)
            current = settings
        elif isinstance(settings, Mapping):
            self.settings_store = AISettingsStore(settings_file)
            current = AISettings.from_mapping(settings)
        else:
            self.settings_store = AISettingsStore(settings_file)
            current = self.settings_store.settings
        if provider is not None or model is not None or base_url is not None:
            current = current.updated(
                **{
                    key: value
                    for key, value in {
                        'provider': provider,
                        'model': model,
                        'base_url': base_url,
                    }.items()
                    if value is not None
                }
            )
        self.settings = current
        self.timeout = self._validate_timeout(timeout)
        if not isinstance(max_response_bytes, int) or not 1 <= max_response_bytes <= MAX_RESPONSE_BYTES:
            raise ValueError('Invalid response limit')
        self.max_response_bytes = max_response_bytes
        self.transport = transport
        self.workspace: Workspace | None = None
        self.context_builder = ProjectContextBuilder()
        self.tool_executor = SafeToolExecutor()
        self._active_lock = threading.RLock()
        self._active_cancel: threading.Event | None = None
        self._active_response: Any = None

    @staticmethod
    def _validate_timeout(value: float) -> float:
        try:
            number = float(value)
        except (TypeError, ValueError) as exc:
            raise ValueError('Invalid timeout') from exc
        if not MIN_TIMEOUT <= number <= MAX_TIMEOUT:
            raise ValueError('Timeout is out of range')
        return number

    @property
    def config(self) -> AISettings:
        return self.settings

    def configure(self, *, provider: str | None = None, model: str | None = None, base_url: str | None = None) -> AISettings:
        values = {'provider': provider, 'model': model, 'base_url': base_url}
        self.settings = self.settings.updated(**{key: value for key, value in values.items() if value is not None})
        self.settings_store.save(self.settings)
        return self.settings

    def save_settings(self) -> AISettings:
        return self.settings_store.save(self.settings)

    def set_project(self, workspace: Any | None) -> None:
        selected = _coerce_workspace(workspace) if workspace is not None else None
        old_root = self.workspace.root if self.workspace is not None else None
        new_root = selected.root if selected is not None else None
        if old_root != new_root:
            self.context_builder.clear_context()
        self.workspace = selected
        self.context_builder.set_project(selected)
        self.tool_executor.set_project(selected)

    def append_context(self, text: str, label: str = 'integration', *, source: str | None = None) -> None:
        self.context_builder.append_context(text, label, source=source)

    def clear_context(self) -> None:
        self.context_builder.clear_context()

    def build_context(self) -> str:
        return self.context_builder.build()

    def context_report(self) -> ProjectContext:
        return self.context_builder.build_report()

    def set_problems_callback(self, callback: Callable[[], Any] | None) -> None:
        self.tool_executor.set_problems_callback(callback)

    def resolve_api_key(self, provider: str | None = None, supplied: str | None = None) -> str | None:
        selected = normalize_provider(provider or self.settings.provider)
        if supplied is not None:
            if not isinstance(supplied, str):
                raise AIConfigurationError('API key must be text')
            return supplied.strip() or None
        for name in PROVIDER_ENV_KEYS[selected]:
            value = os.getenv(name)
            if value and value.strip():
                return value.strip()
        return None

    def _claim_cancel(self) -> threading.Event:
        with self._active_lock:
            if self._active_cancel is not None:
                raise AIAgentError('An AI request is already running')
            event = threading.Event()
            self._active_cancel = event
            return event

    def _release_cancel(self, event: threading.Event) -> None:
        with self._active_lock:
            if self._active_cancel is event:
                self._active_cancel = None

    def cancel(self) -> bool:
        with self._active_lock:
            event = self._active_cancel
            response = self._active_response
            if event is None:
                return False
            event.set()
        if response is not None:
            try:
                response.close()
            except Exception:
                pass
        return True

    @staticmethod
    def _check_cancel(event: threading.Event, external: threading.Event | None) -> None:
        if event.is_set() or (external is not None and external.is_set()):
            raise AICancelled('AI request cancelled')

    @staticmethod
    def _content_text(value: Any) -> str:
        if isinstance(value, str):
            text = value
        elif isinstance(value, Sequence) and not isinstance(value, (bytes, bytearray)):
            pieces: list[str] = []
            for item in value:
                if isinstance(item, str):
                    pieces.append(item)
                elif isinstance(item, Mapping):
                    text_value = item.get('text', item.get('content', ''))
                    if isinstance(text_value, str):
                        pieces.append(text_value)
            text = '\n'.join(pieces)
        elif isinstance(value, Mapping):
            text_value = value.get('text', value.get('content', ''))
            text = text_value if isinstance(text_value, str) else ''
        else:
            text = ''
        text = text.replace('\x00', '')
        return _clip_utf8(text, MAX_MESSAGE_CHARS)[0]

    def _messages(self, messages: str | Sequence[Mapping[str, Any]] | Mapping[str, Any]) -> list[dict[str, str]]:
        if isinstance(messages, str):
            raw: list[Any] = [{'role': 'user', 'content': messages}]
        elif isinstance(messages, Mapping):
            raw = [messages]
        else:
            raw = list(messages)
        if not raw or len(raw) > MAX_MESSAGES:
            raise AIConfigurationError('Invalid message count')
        result: list[dict[str, str]] = []
        for item in raw:
            if not isinstance(item, Mapping):
                raise AIConfigurationError('Each message must be an object')
            role = str(item.get('role', 'user')).strip().casefold()
            if role not in {'user', 'assistant', 'system', 'tool'}:
                role = 'user'
            if role == 'tool':
                role = 'user'
            content = self._content_text(item.get('content', ''))
            if not content:
                continue
            result.append({'role': role, 'content': content})
        if not result:
            raise AIConfigurationError('Message content is empty')
        return result

    @staticmethod
    def _endpoint(settings: AISettings) -> str:
        parsed = urlsplit(settings.base_url)
        path = parsed.path.rstrip('/')
        lower_path = path.casefold()
        if settings.provider == 'openai':
            if lower_path.endswith('/chat/completions'):
                endpoint = path
            elif lower_path.endswith('/v1'):
                endpoint = path + '/chat/completions'
            else:
                endpoint = path + '/v1/chat/completions'
        elif settings.provider == 'ollama':
            if lower_path.endswith('/api/chat'):
                endpoint = path
            elif lower_path.endswith('/api'):
                endpoint = path + '/chat'
            else:
                endpoint = path + '/api/chat'
        elif settings.provider == 'anthropic':
            if lower_path.endswith('/messages'):
                endpoint = path
            elif lower_path.endswith('/v1'):
                endpoint = path + '/messages'
            else:
                endpoint = path + '/v1/messages'
        else:
            if ':generatecontent' in lower_path:
                endpoint = path
            else:
                model = settings.model
                if model.casefold().startswith('models/'):
                    model = model[7:]
                if lower_path.endswith('/v1beta'):
                    endpoint = path + '/models/' + quote(model, safe='') + ':generateContent'
                elif lower_path.endswith('/v1beta/models'):
                    endpoint = path + '/' + quote(model, safe='') + ':generateContent'
                else:
                    endpoint = path + '/v1beta/models/' + quote(model, safe='') + ':generateContent'
        if not endpoint.startswith('/'):
            endpoint = '/' + endpoint
        return urlunsplit((parsed.scheme, parsed.netloc, endpoint, '', ''))

    def _headers(self, provider: str, api_key: str | None) -> dict[str, str]:
        headers = {
            'Accept': 'application/json',
            'Content-Type': 'application/json',
            'User-Agent': 'QEAPP-Studio-AI/1',
        }
        if api_key:
            if provider == 'anthropic':
                headers['x-api-key'] = api_key
                headers['anthropic-version'] = '2023-06-01'
            elif provider == 'gemini':
                headers['x-goog-api-key'] = api_key
            else:
                headers['Authorization'] = 'Bearer ' + api_key
        return headers

    def _request_data(
        self,
        messages: str | Sequence[Mapping[str, Any]] | Mapping[str, Any],
        context: str,
        max_tokens: int,
        api_key: str | None = None,
    ) -> tuple[str, dict[str, str], dict[str, Any]]:
        if not isinstance(context, str):
            context = str(getattr(context, 'text', ''))
        context = _clip_utf8(context.replace('\x00', ''), MAX_CONTEXT_BYTES)[0]
        conversation = self._messages(messages)
        system_parts = [SYSTEM_INSTRUCTIONS]
        if context:
            system_parts.append('Current project context:\n' + context)
        api_messages: list[dict[str, str]] = []
        for message in conversation:
            if message['role'] == 'system':
                system_parts.append(message['content'])
            else:
                api_messages.append(message)
        if not api_messages:
            api_messages = [{'role': 'user', 'content': 'Review the current project context.'}]
        if not 1 <= max_tokens <= MAX_TOKENS:
            raise AIConfigurationError('max_tokens is out of range')
        provider = self.settings.provider
        system_text = '\n\n'.join(system_parts)
        if provider == 'openai':
            payload: dict[str, Any] = {
                'model': self.settings.model,
                'messages': [{'role': 'system', 'content': system_text}, *api_messages],
                'stream': False,
                'max_tokens': max_tokens,
            }
        elif provider == 'ollama':
            payload = {
                'model': self.settings.model,
                'messages': [{'role': 'system', 'content': system_text}, *api_messages],
                'stream': False,
                'options': {'num_predict': max_tokens},
            }
        elif provider == 'anthropic':
            payload = {
                'model': self.settings.model,
                'max_tokens': max_tokens,
                'system': system_text,
                'messages': api_messages,
            }
        else:
            contents = [{'role': 'model' if item['role'] == 'assistant' else 'user', 'parts': [{'text': item['content']}]} for item in api_messages]
            payload = {
                'systemInstruction': {'parts': [{'text': system_text}]},
                'contents': contents,
                'generationConfig': {'maxOutputTokens': max_tokens},
            }
        body, body_truncated = _json_bytes(payload, MAX_REQUEST_BYTES)
        if body_truncated:
            raise AIConfigurationError('AI request exceeds the safe size limit')
        if api_key and self.transport is None and urlsplit(self.settings.base_url).scheme.casefold() == 'http':
            host = urlsplit(self.settings.base_url).hostname or ''
            if not _is_loopback_host(host):
                raise AIConfigurationError('API keys require HTTPS for remote providers')
        if provider in {'anthropic', 'gemini'} and not api_key:
            raise AIConfigurationError(f'{provider} requires an API key from the environment or transient field')
        return self._endpoint(self.settings), self._headers(provider, api_key), payload

    def _invoke_transport(
        self,
        url: str,
        body: bytes,
        headers: Mapping[str, str],
        timeout: float,
        event: threading.Event,
    ) -> Any:
        if self.transport is None:
            return None
        return self.transport(url, body, dict(headers), timeout, event)

    def _read_response(self, response: Any, event: threading.Event, external: threading.Event | None = None) -> bytes:
        headers = getattr(response, 'headers', None)
        content_length = headers.get('Content-Length') if headers is not None else None
        if content_length:
            try:
                if int(content_length) > self.max_response_bytes:
                    raise AIResponseError('AI response exceeds the safe size limit')
            except ValueError:
                pass
        chunks: list[bytes] = []
        total = 0
        while True:
            self._check_cancel(event, external)
            try:
                try:
                    chunk = response.read(min(64 * 1024, self.max_response_bytes - total + 1))
                except TypeError:
                    chunk = response.read()
            except (OSError, ValueError):
                self._check_cancel(event, external)
                raise
            if not chunk:
                break
            if isinstance(chunk, str):
                chunk = chunk.encode('utf-8')
            if not isinstance(chunk, bytes):
                raise AIResponseError('AI response is not binary JSON')
            total += len(chunk)
            if total > self.max_response_bytes:
                raise AIResponseError('AI response exceeds the safe size limit')
            chunks.append(chunk)
        self._check_cancel(event, external)
        return b''.join(chunks)

    def _request_json(
        self,
        url: str,
        body: bytes,
        headers: Mapping[str, str],
        timeout: float,
        event: threading.Event,
        api_key: str | None,
        cancel_event: threading.Event | None = None,
    ) -> Mapping[str, Any]:
        external = self._invoke_transport(url, body, headers, timeout, event)
        if external is not None:
            if isinstance(external, Mapping):
                _, truncated = _json_bytes(external, self.max_response_bytes)
                if truncated:
                    raise AIResponseError('AI response exceeds the safe size limit')
                return external
            if isinstance(external, bytes):
                raw = external
            elif isinstance(external, str):
                raw = external.encode('utf-8')
            else:
                raise AIResponseError('AI transport returned an invalid response')
            if len(raw) > self.max_response_bytes:
                raise AIResponseError('AI response exceeds the safe size limit')
        else:
            request = Request(url, data=body, headers=dict(headers), method='POST')
            opener = build_opener(_NoRedirectHandler())
            response = None
            try:
                response = opener.open(request, timeout=timeout)
                with self._active_lock:
                    self._active_response = response
                status = getattr(response, 'status', None) or response.getcode()
                if status is not None and not 200 <= int(status) < 300:
                    raise AIHTTPError(f'AI provider returned HTTP {int(status)}')
                raw = self._read_response(response, event, cancel_event)
            except HTTPError as exc:
                try:
                    error_body = exc.read(4096)
                except Exception:
                    error_body = b''
                message = ''
                if error_body:
                    try:
                        value = json.loads(error_body.decode('utf-8', errors='replace'))
                        if isinstance(value, Mapping):
                            message = str(value.get('error', value.get('message', '')))
                    except (TypeError, ValueError):
                        message = ''
                detail = _redact(message or exc.reason or 'provider request failed', api_key)
                raise AIHTTPError(f'AI provider returned HTTP {exc.code}: {detail}') from None
            except (socket.timeout, TimeoutError) as exc:
                raise AITimeoutError('AI provider request timed out') from exc
            except URLError as exc:
                if isinstance(exc.reason, (socket.timeout, TimeoutError)):
                    raise AITimeoutError('AI provider request timed out') from exc
                raise AIConnectionError(_redact(exc.reason, api_key)) from exc
            finally:
                with self._active_lock:
                    if self._active_response is response:
                        self._active_response = None
                if response is not None:
                    try:
                        response.close()
                    except Exception:
                        pass
        try:
            value = json.loads(raw.decode('utf-8'))
        except (UnicodeDecodeError, TypeError, ValueError) as exc:
            raise AIResponseError('AI provider returned invalid JSON') from exc
        if not isinstance(value, Mapping):
            raise AIResponseError('AI provider returned an invalid JSON object')
        return value

    @staticmethod
    def _response_text(provider: str, value: Mapping[str, Any]) -> str:
        text = ''
        if provider in {'openai', 'ollama'}:
            message_value = value.get('message')
            if isinstance(message_value, Mapping):
                text = str(message_value.get('content', ''))
            choices = value.get('choices')
            if isinstance(choices, list) and choices and isinstance(choices[0], Mapping):
                first = choices[0]
                message = first.get('message')
                if isinstance(message, Mapping):
                    text = str(message.get('content', ''))
                if not text:
                    text = str(first.get('text', ''))
            if not text:
                text = str(value.get('response', ''))
            if not text:
                text = str(value.get('output_text', ''))
        elif provider == 'anthropic':
            content = value.get('content')
            if isinstance(content, str):
                text = content
            elif isinstance(content, list):
                text = '\n'.join(str(item.get('text', '')) for item in content if isinstance(item, Mapping))
            if not text:
                text = str(value.get('completion', ''))
        else:
            candidates = value.get('candidates')
            if isinstance(candidates, list) and candidates and isinstance(candidates[0], Mapping):
                content = candidates[0].get('content')
                parts = content.get('parts') if isinstance(content, Mapping) else None
                if isinstance(parts, list):
                    text = '\n'.join(str(item.get('text', '')) for item in parts if isinstance(item, Mapping))
            if not text:
                text = str(value.get('text', ''))
        text = text.replace('\x00', '')
        text, _ = _clip_utf8(text, MAX_MESSAGE_CHARS * 2)
        if not text.strip():
            raise AIResponseError('AI provider returned no text')
        return text

    def send(
        self,
        messages: str | Sequence[Mapping[str, Any]] | Mapping[str, Any],
        *,
        context: str | ProjectContext | None = None,
        api_key: str | None = None,
        cancel_event: threading.Event | None = None,
        timeout: float | None = None,
        max_tokens: int = DEFAULT_MAX_TOKENS,
    ) -> str:
        selected_timeout = self.timeout if timeout is None else self._validate_timeout(timeout)
        event = self._claim_cancel()
        resolved_key: str | None = None
        try:
            resolved_key = self.resolve_api_key(supplied=api_key)
            self._check_cancel(event, cancel_event)
            if context is None:
                context_text = self.build_context()
                if self.workspace is None and context_text == 'Project: none\n':
                    context_text = ''
            else:
                context_text = context.text if isinstance(context, ProjectContext) else context
            safe_messages = _redact_value(messages, resolved_key) if resolved_key else messages
            safe_context = _redact_value(context_text, resolved_key) if resolved_key else context_text
            url, headers, payload = self._request_data(safe_messages, safe_context, max_tokens, resolved_key)
            if resolved_key and self.transport is None and urlsplit(self.settings.base_url).scheme.casefold() == 'http':
                host = urlsplit(self.settings.base_url).hostname or ''
                if not _is_loopback_host(host):
                    raise AIConfigurationError('API keys require HTTPS for remote providers')
            body, truncated = _json_bytes(payload, MAX_REQUEST_BYTES)
            if truncated:
                raise AIConfigurationError('AI request exceeds the safe size limit')
            value = self._request_json(url, body, headers, selected_timeout, event, resolved_key, cancel_event)
            self._check_cancel(event, cancel_event)
            text = self._response_text(self.settings.provider, value)
            if resolved_key:
                text = text.replace(resolved_key, '[REDACTED]')
            return text
        except AICancelled:
            raise
        except (AIConfigurationError, AITimeoutError, AIConnectionError, AIHTTPError, AIResponseError):
            raise
        except (socket.timeout, TimeoutError) as exc:
            raise AITimeoutError('AI provider request timed out') from exc
        except URLError as exc:
            if isinstance(exc.reason, (socket.timeout, TimeoutError)):
                raise AITimeoutError('AI provider request timed out') from exc
            raise AIConnectionError(_redact(exc.reason, resolved_key)) from exc
        except Exception as exc:
            raise AIAgentError(_redact('AI request failed: ' + str(exc), resolved_key)) from None
        finally:
            self._release_cancel(event)

    def request(self, messages: str | Sequence[Mapping[str, Any]] | Mapping[str, Any], **kwargs: Any) -> str:
        return self.send(messages, **kwargs)

    def complete(self, prompt: str | Sequence[Mapping[str, Any]] | Mapping[str, Any] | None = None, **kwargs: Any) -> str:
        if prompt is None:
            prompt = kwargs.pop('messages', '')
        return self.send(prompt, **kwargs)

    def send_message(self, message: str, **kwargs: Any) -> str:
        return self.send(message, **kwargs)

    def execute_tool(self, call: ToolCall | Mapping[str, Any] | str, arguments: Mapping[str, Any] | None = None) -> ToolResult:
        return self.tool_executor.execute(call, arguments)

    def safe_error(self, error: BaseException, api_key: str | None = None) -> str:
        return _redact(str(error) or error.__class__.__name__, api_key)

    @staticmethod
    def _redact_tool_result(result: ToolResult, api_key: str | None) -> ToolResult:
        if not api_key:
            return result
        return replace(
            result,
            data=_redact_value(result.data, api_key),
            error=_redact(result.error, api_key),
        )

    def respond(
        self,
        messages: str | Sequence[Mapping[str, Any]] | Mapping[str, Any],
        *,
        context: str | ProjectContext | None = None,
        api_key: str | None = None,
        cancel_event: threading.Event | None = None,
        timeout: float | None = None,
        max_tokens: int = DEFAULT_MAX_TOKENS,
        max_tool_rounds: int = 1,
    ) -> AgentResponse:
        if not isinstance(max_tool_rounds, int) or not 0 <= max_tool_rounds <= 2:
            raise ValueError('Invalid tool round limit')
        redaction_key = self.resolve_api_key(supplied=api_key)
        current = messages
        text = self.send(
            current,
            context=context,
            api_key=api_key,
            cancel_event=cancel_event,
            timeout=timeout,
            max_tokens=max_tokens,
        )
        all_results: list[ToolResult] = []
        all_errors: list[str] = []
        for round_index in range(max_tool_rounds + 1):
            calls, errors = parse_tool_blocks(text)
            all_errors.extend(errors)
            if not calls or round_index >= max_tool_rounds:
                break
            results = tuple(self._redact_tool_result(self.tool_executor.execute(call), redaction_key) for call in calls)
            all_results.extend(results)
            result_text = _clip_utf8('\n'.join(result.as_text() for result in results), MAX_CONTEXT_BYTES)[0]
            if isinstance(current, str):
                history: list[Mapping[str, Any]] = [{'role': 'user', 'content': current}]
            elif isinstance(current, Mapping):
                history = [current]
            else:
                history = list(current)
            if len(history) > MAX_MESSAGES - 2:
                history = history[-(MAX_MESSAGES - 2):]
            history.extend([
                {'role': 'assistant', 'content': text},
                {'role': 'user', 'content': 'Tool results:\n' + result_text},
            ])
            current = history
            text = self.send(
                current,
                context=context,
                api_key=api_key,
                cancel_event=cancel_event,
                timeout=timeout,
                max_tokens=max_tokens,
            )
        return AgentResponse(text, tuple(all_results), tuple(all_errors))


AiAgent = AIAgent
AIChatAgent = AIAgent
SafeProjectContextBuilder = ProjectContextBuilder
ToolExecutor = SafeToolExecutor
AIConfig = AISettings
AIConfigStore = AISettingsStore
