from __future__ import annotations

import os
from pathlib import Path
import shutil


def find_host_tool(name: str) -> str:
    if not isinstance(name, str) or not name.strip():
        raise ValueError('Host tool name is required')
    tool = name.strip()
    override = os.environ.get('QEAPP_HOST_TOOLCHAIN', '').strip()
    candidates = []
    if override:
        override_path = Path(override).expanduser()
        candidates.extend((override_path / tool, override_path / (tool + '.exe')))
    mingw = os.environ.get('QEAPP_MINGW_BIN', '').strip()
    if mingw:
        mingw_path = Path(mingw).expanduser()
        candidates.extend((mingw_path / tool, mingw_path / (tool + '.exe')))
    if os.name == 'nt':
        for base in (r'C:\msys64\mingw64\bin', r'C:\msys64\ucrt64\bin',
                     r'C:\msys64\clang64\bin', r'C:\mingw64\bin'):
            path = Path(base)
            candidates.extend((path / tool, path / (tool + '.exe')))
    found = shutil.which(tool)
    if found:
        return str(Path(found).resolve())
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate.resolve())
    raise RuntimeError(f'Host compiler not found: {tool}. Set QEAPP_HOST_TOOLCHAIN or add it to PATH.')
