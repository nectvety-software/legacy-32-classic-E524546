"""Unique QEAPP project App IDs (LuaS30-style random, no collisions)."""
from __future__ import annotations

import json
import re
import secrets
from pathlib import Path

__all__ = [
    'APP_ID_RE', 'collect_app_ids', 'generate_unique_app_id', 'is_valid_app_id',
    'suggested_app_id',
]

APP_ID_RE = re.compile(r'[a-z0-9_-]{1,24}')
_ID_ALPHABET = 'abcdefghijklmnopqrstuvwxyz0123456789'
_PROJECT_FILE = 'qeapp.project.json'


def is_valid_app_id(value: str) -> bool:
    return bool(value) and bool(APP_ID_RE.fullmatch(value))


def _read_id(descriptor: Path) -> str | None:
    try:
        payload = json.loads(descriptor.read_text(encoding='utf-8-sig'))
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError):
        return None
    if not isinstance(payload, dict):
        return None
    app_id = payload.get('id')
    return app_id if isinstance(app_id, str) and app_id else None


def collect_app_ids(projects_root: Path, *, exclude: Path | None = None) -> set[str]:
    """Return App IDs declared by project manifests under *projects_root*."""
    root = Path(projects_root).expanduser()
    used: set[str] = set()
    if not root.is_dir():
        return used
    excluded = Path(exclude).resolve() if exclude else None
    try:
        candidates = root.rglob(_PROJECT_FILE)
    except OSError:
        return used
    for descriptor in candidates:
        try:
            project_root = descriptor.parent.resolve()
            if excluded and project_root == excluded:
                continue
            app_id = _read_id(descriptor)
            if app_id:
                used.add(app_id)
        except OSError:
            continue
    return used


def suggested_app_id(projects_root: Path, *, exclude: Path | None = None) -> str:
    """Random App ID matching ``[a-z0-9_-]{1,24}`` and unused in *projects_root*."""
    used = collect_app_ids(projects_root, exclude=exclude)
    for _ in range(256):
        candidate = 'app_' + ''.join(secrets.choice(_ID_ALPHABET) for _ in range(8))
        if candidate not in used:
            return candidate
    for index in range(1, 10_000):
        candidate = f'app_{index}'
        if candidate not in used and is_valid_app_id(candidate):
            return candidate
    raise ValueError('No free QEAPP App ID remains in the configured range')


# Alias used by the New Project dialog / LuaS30 parity naming.
generate_unique_app_id = suggested_app_id
