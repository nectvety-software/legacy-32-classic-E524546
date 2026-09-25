#!/usr/bin/env python3
"""Find the separate VQEAF-OS checkout. Never vendor firmware into Studio.

Priority:
1. ``QEAPP_FIRMWARE_ROOT`` environment variable
2. User Settings JSON ``firmware_root`` (when ``settings`` is passed / auto)
3. Sibling ``../VQEAF-OS``
4. Embedded ``firmware/VQEAF-OS`` (monorepo only)

Never treat the Studio root itself as firmware source.
"""
from __future__ import annotations

import json
import os
from pathlib import Path

STUDIO_ROOT = Path(__file__).resolve().parents[1]

# Files that prove a checkout is a real VQEAF-OS firmware tree.
_REQUIRED = ('platformio.ini',)
_SIGNER = Path('tools') / 'build_qeapp.py'
_LUA_BETA = Path('src') / 'lua' / 'QeLuaRuntime.cpp'


class FirmwareRootError(ValueError):
    """Invalid or missing firmware source."""


def _is_studio_root(path: Path) -> bool:
    try:
        resolved = path.resolve()
    except OSError:
        return False
    if resolved == STUDIO_ROOT.resolve():
        return True
    # A folder that only looks like Studio (has run_studio.py + studio/) is Studio.
    return (resolved / 'run_studio.py').is_file() and (resolved / 'studio' / 'gui' / 'window.py').is_file()


def inspect_firmware_root(path: Path | str) -> dict:
    """Return a structured check of *path* as a firmware checkout (no secrets)."""
    raw = Path(os.path.expandvars(str(path or ''))).expanduser()
    result = {
        'path': str(raw),
        'resolved': '',
        'exists': False,
        'is_studio': False,
        'has_platformio': False,
        'has_signer': False,
        'has_lua_beta': False,
        'ready_for_build': False,
        'ready_for_lua_build': False,
        'error_code': '',
        'message': '',
    }
    if not str(path or '').strip():
        result['error_code'] = 'OS_ROOT_MISSING'
        result['message'] = 'Firmware source folder is empty'
        return result
    try:
        resolved = raw.resolve()
    except OSError:
        result['error_code'] = 'OS_ROOT_INVALID'
        result['message'] = f'Cannot resolve path: {raw}'
        return result
    result['resolved'] = str(resolved)
    result['exists'] = resolved.is_dir()
    if not result['exists']:
        result['error_code'] = 'OS_ROOT_INVALID'
        result['message'] = f'Not a directory: {resolved}'
        return result
    result['is_studio'] = _is_studio_root(resolved)
    if result['is_studio']:
        result['error_code'] = 'OS_ROOT_IS_STUDIO'
        result['message'] = 'Path points at QEAPP-Studio, not VQEAF-OS'
        return result
    result['has_platformio'] = (resolved / 'platformio.ini').is_file()
    result['has_signer'] = (resolved / _SIGNER).is_file()
    result['has_lua_beta'] = (resolved / _LUA_BETA).is_file()
    if not result['has_platformio']:
        result['error_code'] = 'OS_ROOT_INVALID'
        result['message'] = 'Missing platformio.ini (not a VQEAF-OS checkout)'
        return result
    if not result['has_signer']:
        result['error_code'] = 'SIGNER_MISSING'
        result['message'] = f'Missing {_SIGNER.as_posix()}'
        return result
    result['ready_for_build'] = True
    result['ready_for_lua_build'] = result['has_lua_beta']
    result['message'] = 'OK'
    return result


def _settings_firmware_root() -> str:
    try:
        from studio.core.paths import resolve_config_file
        settings = resolve_config_file('settings.json')
    except Exception:
        base = Path(os.getenv('APPDATA') or os.getenv('XDG_CONFIG_HOME') or Path.home() / '.config')
        settings = base / 'QEAPP-Studio' / 'config' / 'settings.json'
        if not settings.is_file():
            settings = base / 'QEAPPStudio' / 'settings.json'
    try:
        payload = json.loads(settings.read_text(encoding='utf-8-sig'))
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError):
        return ''
    if not isinstance(payload, dict):
        return ''
    value = payload.get('firmware_root')
    return str(value) if isinstance(value, str) else ''


def firmware_root(*, required: bool = False, settings_path: Path | None = None) -> Path | None:
    """Return the best firmware checkout path, or None when unavailable."""
    candidates: list[str] = []
    env = os.environ.get('QEAPP_FIRMWARE_ROOT')
    if env and env.strip():
        candidates.append(env.strip())
    if settings_path is not None:
        try:
            payload = json.loads(Path(settings_path).read_text(encoding='utf-8-sig'))
            value = payload.get('firmware_root') if isinstance(payload, dict) else ''
            if isinstance(value, str) and value.strip():
                candidates.append(value.strip())
        except (OSError, UnicodeError, ValueError, json.JSONDecodeError):
            pass
    else:
        settings_value = _settings_firmware_root()
        if settings_value:
            candidates.append(settings_value)

    for index, candidate in enumerate(candidates):
        report = inspect_firmware_root(candidate)
        if report['ready_for_build']:
            return Path(report['resolved'])
        # QEAPP_FIRMWARE_ROOT is an explicit override: if it is set but wrong,
        # do not silently fall through to settings/discovery when required.
        if required and index == 0 and env and env.strip() and report.get('error_code'):
            raise FirmwareRootError(report['error_code'] + ': ' + report['message'])

    # Explicit configuration that is wrong must not be silently ignored when required.
    if required and candidates:
        report = inspect_firmware_root(candidates[0])
        if report.get('error_code'):
            raise FirmwareRootError(report['error_code'] + ': ' + report['message'])

    for fallback in (STUDIO_ROOT.parent / 'VQEAF-OS', STUDIO_ROOT / 'firmware' / 'VQEAF-OS'):
        report = inspect_firmware_root(fallback)
        if report['ready_for_build']:
            return Path(report['resolved'])

    if required:
        for candidate in candidates:
            report = inspect_firmware_root(candidate)
            if report['error_code']:
                raise FirmwareRootError(report['error_code'] + ': ' + report['message'])
        raise FirmwareRootError(
            'OS_ROOT_MISSING: Clone VQEAF-OS next to QEAPP-Studio, set QEAPP_FIRMWARE_ROOT, '
            'or choose the folder in Settings → Build / Firmware'
        )
    return None


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--require', action='store_true')
    ap.add_argument('--json', action='store_true', help='Print inspect_firmware_root for resolved/declared path')
    ap.add_argument('--path', help='Inspect this folder instead of auto-resolving')
    args = ap.parse_args()
    try:
        if args.path:
            report = inspect_firmware_root(args.path)
        else:
            root = firmware_root(required=args.require)
            report = inspect_firmware_root(root) if root else {
                'path': '', 'error_code': 'OS_ROOT_MISSING',
                'message': 'Firmware not installed (host IDE can still run)',
                'ready_for_build': False, 'ready_for_lua_build': False,
            }
        if args.json:
            print(json.dumps(report, indent=2))
        else:
            if report.get('ready_for_build'):
                print(report['resolved'])
            else:
                print(report.get('message') or 'Firmware not installed (host IDE can still run)')
                if not args.require:
                    raise SystemExit(0)
                raise SystemExit(2)
        if args.require and not report.get('ready_for_build'):
            raise SystemExit(2)
    except FirmwareRootError as exc:
        ap.exit(2, f'ERROR: {exc}\n')
