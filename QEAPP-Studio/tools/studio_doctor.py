#!/usr/bin/env python3
"""Read-only diagnosis by default; --apply repairs corrupt user settings only.

``--json`` prints structured checks. ``export_bundle()`` writes a redacted
diagnostic zip/text under logs/ for Help → Export Diagnostic Bundle.
"""
from __future__ import annotations

import argparse
import json
import platform
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from studio.core import repair  # noqa: E402

try:
    from tools.host_toolchain import find_host_tool
except ModuleNotFoundError:
    from host_toolchain import find_host_tool  # type: ignore


def _redact(text: str) -> str:
    import re
    text = re.sub(r'-----BEGIN [A-Z ]*PRIVATE KEY-----[\s\S]*?-----END [A-Z ]*PRIVATE KEY-----',
                  '[REDACTED PRIVATE KEY]', text)
    text = re.sub(r'(?i)(sign[_-]?key|private[_-]?key|password|api[_-]?key)\s*[=:]\s*\S+',
                  r'\1=[REDACTED]', text)
    return text


def collect_environment() -> dict:
    import os
    info = {
        'python': sys.version.split()[0],
        'platform': platform.platform(),
        'machine': platform.machine(),
        'studio_root': str(ROOT),
        'cwd': str(Path.cwd()),
    }
    try:
        import PySide6
        info['pyside6'] = getattr(PySide6, '__version__', 'unknown')
    except ImportError:
        info['pyside6'] = None
    try:
        from tools.studio_firmware import firmware_root, inspect_firmware_root
        root = firmware_root()
        info['firmware_root'] = str(root) if root else None
        info['firmware_check'] = inspect_firmware_root(root) if root else inspect_firmware_root(os.environ.get('QEAPP_FIRMWARE_ROOT', ''))
        # Never echo key paths in full later; firmware path is not secret.
    except Exception as exc:
        info['firmware_error'] = str(exc)
    for tool in ('gcc', 'g++'):
        try:
            info[tool] = bool(find_host_tool(tool))
        except Exception:
            info[tool] = False
    return info


def export_bundle(directory: Path | None = None) -> Path:
    """Write a redacted diagnostic bundle under logs/. Returns zip path."""
    out_dir = directory or (ROOT / 'logs')
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    zip_path = out_dir / f'diagnostic-bundle-{stamp}.zip'
    checks = repair.inspect()
    payload = {
        'generated_utc': stamp,
        'environment': collect_environment(),
        'checks': [{'name': c.name, 'status': c.status, 'detail': c.detail} for c in checks],
    }
    text = json.dumps(payload, indent=2, ensure_ascii=False)
    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr('diagnostics.json', _redact(text))
        zf.writestr('README.txt',
                    'QEAPP Studio diagnostic bundle\n'
                    'Secrets/private keys are redacted. Firmware device logs are NOT included.\n'
                    'Host VM screenshots are PC simulation only.\n')
        for name in ('logs/launcher.log', 'logs/gui-crash.log', 'logs/gui-errors.log'):
            src = ROOT / name
            if src.is_file():
                zf.writestr(name, _redact(src.read_text(encoding='utf-8', errors='replace')[-20000:]))
    return zip_path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--apply', action='store_true', help='Back up and reset invalid settings.json only')
    p.add_argument('--json', action='store_true')
    p.add_argument('--export-bundle', action='store_true', help='Write redacted diagnostic zip under logs/')
    a = p.parse_args()
    if a.export_bundle:
        path = export_bundle()
        print(path)
        return 0
    checks = repair.inspect()
    if a.json:
        print(json.dumps({
            'environment': collect_environment(),
            'checks': [{'name': c.name, 'status': c.status, 'detail': c.detail} for c in checks],
        }, indent=2, ensure_ascii=False))
    else:
        for c in checks:
            print(f'[{c.status}] {c.name}: {c.detail}')
    if a.apply:
        backup = repair.apply()
        print('BACKUP:', str(backup) if backup else 'No corrupt settings; nothing changed')
    return 2 if any(c.status == 'FAIL' for c in checks) else 0


if __name__ == '__main__':
    raise SystemExit(main())
