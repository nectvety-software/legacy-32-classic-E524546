"""Standard Lua Application scaffold for QEAPP-Studio.

Single generator used by GUI New Project and ``tools/qstudio.py init``.
Layout follows docs/agents/PROMPT.md (manifest schema, assets, tests, docs).
"""
from __future__ import annotations

import json
import os
import re
import secrets
import shutil
from pathlib import Path

__all__ = [
    'STANDARD_LAYOUT', 'ScaffoldError', 'create_standard_lua_project',
    'create_project', 'validate_app_id', 'validate_display_name', 'validate_project_name',
]

APP_ID_RE = re.compile(r'[a-z0-9_-]{1,24}')
VERSION_DEFAULT = '1.0.0'
_MAX_LUA_BYTES = 64 * 1024

STANDARD_LAYOUT = (
    'qeapp.project.json',
    'main.lua',
    'README.md',
    'CHANGELOG.md',
    'assets/icon.png',
    'assets/sprites/.gitkeep',
    'assets/fonts/.gitkeep',
    'assets/sounds/.gitkeep',
    'tests/input_replay.json',
    'tests/test_cases.json',
    'docs/DEVELOPMENT.md',
)

_MAIN_LUA = """-- Standard Lua Application (Beta) — QEAPP Studio
-- Host PC preview only. Device run needs vqeaf_lua_beta + matching trust key.
local count = 0

function on_key(key, down)
  if not down then return end
  if key == 'start' then
    count = count + 1
  end
end

function on_update(dt)
  -- Bounded update. No file/network/require/dofile.
end

function on_draw()
  engine.clear(0x0924)
  engine.text(12, 16, '{title}', 0xFFFF)
  engine.text(12, 40, 'START TO COUNT', 0xFFFF)
  engine.text(12, 64, tostring(count), 0x07E0)
end
"""

_INPUT_REPLAY = [
    {"frame": 1, "key": "start", "down": True},
    {"frame": 2, "key": "start", "down": False},
]

_TEST_CASES = {
    "note": "Documented manual cases only until a dedicated runner exists.",
    "status": "MANUAL/NOT_RUN",
    "cases": [
        {"id": "TC01", "title": "Start increments counter", "steps": "Open host preview, press START", "expected": "Counter increases", "status": "MANUAL/NOT_RUN"},
        {"id": "TC02", "title": "Draw shows title", "steps": "Run on_draw", "expected": "Title text visible", "status": "MANUAL/NOT_RUN"},
    ],
}


class ScaffoldError(ValueError):
    """User-facing project creation error (no partial project left behind)."""


def validate_app_id(app_id: str) -> str:
    text = str(app_id or '').strip()
    if not APP_ID_RE.fullmatch(text):
        raise ScaffoldError('App ID phải khớp [a-z0-9_-]{1,24}')
    return text


def validate_display_name(name: str) -> str:
    text = str(name or '').strip()
    if not text:
        raise ScaffoldError('Display name không được để trống')
    if len(text) > 40:
        raise ScaffoldError('Display name tối đa 40 ký tự')
    if not text.isascii() or any(ord(c) < 32 or ord(c) > 126 for c in text):
        raise ScaffoldError('Display name phải là ASCII in được (1..40)')
    if any(c in '<>:"/\\|?*' for c in text):
        raise ScaffoldError('Display name không được chứa ký tự đường dẫn Windows')
    return text


def validate_project_name(name: str) -> str:
    text = str(name or '').strip()
    if not text:
        raise ScaffoldError('Project name không được để trống')
    if len(text) > 40:
        raise ScaffoldError('Project name tối đa 40 ký tự')
    if not text.isascii() or any(ord(c) < 32 or ord(c) > 126 for c in text):
        raise ScaffoldError('Project name phải là ASCII in được (1..40)')
    if any(c in '<>:"/\\|?*' for c in text):
        raise ScaffoldError('Project name không được chứa ký tự đường dẫn Windows')
    if text != text.rstrip(' .'):
        raise ScaffoldError('Project name không được kết thúc bằng dấu chấm hoặc khoảng trắng')
    reserved = {'CON', 'PRN', 'AUX', 'NUL'} | {f'COM{i}' for i in range(1, 10)} | {f'LPT{i}' for i in range(1, 10)}
    if text.split('.')[0].upper() in reserved:
        raise ScaffoldError(f'"{text}" là tên dành riêng của Windows')
    return text


def _write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')


def _png_chunk(tag: bytes, data: bytes) -> bytes:
    import struct
    import zlib
    return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)


def _write_icon(path: Path) -> None:
    """Write a valid 32×32 RGB PNG (stdlib only; Pillow optional)."""
    import struct
    import zlib
    path.parent.mkdir(parents=True, exist_ok=True)
    size = 32
    rows = bytearray()
    for y in range(size):
        rows.append(0)  # filter: None
        for x in range(size):
            # dark navy + orange border/crosshair (matches Studio accent)
            edge = x < 3 or y < 3 or x >= size - 3 or y >= size - 3
            mark = 12 <= x <= 19 and 12 <= y <= 19
            if edge or mark:
                rows.extend((255, 138, 0))
            else:
                rows.extend((17, 17, 34))
    ihdr = struct.pack('>IIBBBBB', size, size, 8, 2, 0, 0, 0)
    png = (
        b'\x89PNG\r\n\x1a\n'
        + _png_chunk(b'IHDR', ihdr)
        + _png_chunk(b'IDAT', zlib.compress(bytes(rows), 9))
        + _png_chunk(b'IEND', b'')
    )
    path.write_bytes(png)


def _manifest(app_id: str, name: str) -> dict:
    return {
        'project_format': 1,
        'id': app_id,
        'name': name,
        'version': VERSION_DEFAULT,
        'type': 'lua',
        'content': 'main.lua',
        'icon': 'assets/icon.png',
    }


def _title_for_draw(display_name: str) -> str:
    # engine.text is a small bitmap font; keep the title short and safe.
    compact = ''.join(ch if 32 < ord(ch) < 127 else ' ' for ch in display_name).strip()
    return (compact or 'MY APPLICATION')[:18].upper()


def _materialize(root: Path, app_id: str, display_name: str) -> None:
    root.mkdir(parents=True, exist_ok=True)
    _write_text(root / 'qeapp.project.json', json.dumps(_manifest(app_id, display_name), indent=2, ensure_ascii=True) + '\n')
    _write_text(root / 'main.lua', _MAIN_LUA.format(title=_title_for_draw(display_name)))
    if root.joinpath('main.lua').stat().st_size > _MAX_LUA_BYTES:
        raise ScaffoldError('main.lua vượt giới hạn 64 KiB')
    _write_text(root / 'README.md', _readme(display_name, app_id))
    _write_text(root / 'CHANGELOG.md', f'# {display_name}\n\n## 1.0.0\n\n- Initial Standard Lua Application scaffold.\n')
    _write_icon(root / 'assets' / 'icon.png')
    for folder in ('sprites', 'fonts', 'sounds'):
        keep = root / 'assets' / folder / '.gitkeep'
        keep.parent.mkdir(parents=True, exist_ok=True)
        keep.write_bytes(b'')
    _write_text(root / 'tests' / 'input_replay.json', json.dumps(_INPUT_REPLAY, indent=2) + '\n')
    _write_text(root / 'tests' / 'test_cases.json', json.dumps(_TEST_CASES, indent=2) + '\n')
    _write_text(root / 'docs' / 'DEVELOPMENT.md', _development(display_name))


def _readme(display_name: str, app_id: str) -> str:
    return f"""# {display_name}

Standard Lua Application (Beta) scaffold for QEAPP Studio.

- **App ID:** `{app_id}`
- **Entry:** `main.lua` (Lua 5.4 host API: `on_key`, `on_update`, `on_draw`, `engine.*`)
- **Screen:** 240×320 host preview (PC). This is **not** a full ESP32-S3 / Symbian / MRE emulator.

## Controls

| Key | Action |
| --- | --- |
| START | Increment counter |

## Run / test

```powershell
py -3 tools\\qstudio.py validate .
py -3 tools\\qstudio.py lua-preview . --frames 8 --replay tests/input_replay.json -o preview.png
```

Host preview PASS does **not** prove device firmware. A signed `.qeapp` of type `lua`
still requires **vqeaf_lua_beta** firmware and the matching publisher trust key.
Stock firmware only accepts supported web/text packages with a valid signature.

## Structure

See `docs/DEVELOPMENT.md`. `assets/sprites|fonts|sounds` are design sources only;
the current packer ships `main.lua` + optional 32×32 `assets/icon.png`.
"""


def _development(display_name: str) -> str:
    return f"""# {display_name} — Development notes

## Project rules

- Keep `main.lua` ≤ 64 KiB, UTF-8, no NUL, no `require`/`dofile`.
- Use only host engine APIs (`engine.clear`, `engine.text`, `engine.rect`, `engine.width`, `engine.height`, keys `start/up/down/left/right/option`).
- `tests/input_replay.json` is `{{frame, key, down}}` for the deterministic host replay.
- `tests/test_cases.json` is documentation (`MANUAL/NOT_RUN`) until a dedicated runner exists.

## Device boundary

| Surface | Supported today |
| --- | --- |
| PC Lua host / Virtual Phone | Yes (bounded VM) |
| Stock VQEAF-OS QEAPP/2 | `web` / `text` with valid signature |
| `type=lua` on device | Opt-in `vqeaf_lua_beta` + beta trust key only |

Do not treat host screenshots as ST7789 device photos.
"""


def _validate_created(root: Path) -> None:
    missing = [rel for rel in STANDARD_LAYOUT if not (root / rel).is_file()]
    if missing:
        raise ScaffoldError('Thiếu tệp scaffold: ' + ', '.join(missing))
    payload = json.loads((root / 'qeapp.project.json').read_text(encoding='utf-8'))
    if set(payload) != {'project_format', 'id', 'name', 'version', 'type', 'content', 'icon'}:
        raise ScaffoldError('Manifest scaffold phải đúng 7 khóa schema')
    lua = (root / 'main.lua').read_bytes()
    if b'\x00' in lua or len(lua) > _MAX_LUA_BYTES:
        raise ScaffoldError('main.lua không hợp lệ')
    icon = (root / 'assets' / 'icon.png').read_bytes()
    if icon[:8] != b'\x89PNG\r\n\x1a\n':
        raise ScaffoldError('assets/icon.png phải là PNG')
    # IHDR width/height at bytes 16..24
    import struct
    width, height = struct.unpack('>II', icon[16:24])
    if (width, height) != (32, 32):
        raise ScaffoldError('assets/icon.png phải là PNG 32×32')


def create_standard_lua_project(
    destination: Path,
    app_id: str,
    display_name: str,
    *,
    project_name: str | None = None,
    used_app_ids: set[str] | None = None,
) -> Path:
    """Create a Standard Lua Application at *destination*.

    Writes into a sibling staging folder first, validates, then renames.
    Never overwrites an existing destination.
    """
    app_id = validate_app_id(app_id)
    display_name = validate_display_name(display_name)
    destination = Path(destination)
    # destination is always the project folder. Validate its name (or explicit project_name).
    folder_name = validate_project_name(project_name or destination.name)
    if project_name and destination.name and project_name != destination.name:
        raise ScaffoldError('Project name không khớp tên thư mục đích')
    if used_app_ids and app_id in used_app_ids:
        raise ScaffoldError(f'App ID đã tồn tại: {app_id}')

    if destination.exists():
        raise ScaffoldError('Thư mục dự án đã tồn tại; không ghi đè')

    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = destination.parent / f'.{destination.name}.tmp-{secrets.token_hex(4)}'
    try:
        if staging.exists():
            raise ScaffoldError('Staging path already exists')
        _materialize(staging, app_id, display_name)
        _validate_created(staging)
        os.rename(staging, destination)
    except Exception:
        if staging.exists():
            shutil.rmtree(staging, ignore_errors=True)
        raise
    return destination


# Back-compat name used by tools/qstudio.py and window.py.
create_project = create_standard_lua_project
