#!/usr/bin/env python3
"""Install a verified official Lua 5.4.8 source set for the firmware library."""
from __future__ import annotations

import argparse
import hashlib
import io
from pathlib import Path
import tarfile
import urllib.request

URL = 'https://www.lua.org/ftp/lua-5.4.8.tar.gz'
SHA256 = '4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae'
CORE = {
    'lapi.c', 'lcode.c', 'lctype.c', 'ldebug.c', 'ldo.c', 'ldump.c', 'lfunc.c',
    'lgc.c', 'llex.c', 'lmem.c', 'lobject.c', 'lopcodes.c', 'lparser.c',
    'lstate.c', 'lstring.c', 'ltable.c', 'ltm.c', 'lundump.c', 'lvm.c', 'lzio.c',
    'lauxlib.c', 'lbaselib.c', 'lmathlib.c', 'lstrlib.c', 'ltablib.c', 'lutf8lib.c',
}


def _embedded_license(header: bytes) -> bytes:
    text = header.decode('utf-8')
    start = text.find('/******************************************************************************')
    end_marker = '******************************************************************************/'
    end = text.find(end_marker, start)
    if start < 0 or end < 0:
        raise ValueError('Lua license block is missing')
    return text[start:end + len(end_marker)].encode('utf-8') + b'\n'


def install(firmware: Path, archive: Path | None = None) -> int:
    firmware = firmware.resolve()
    if not (firmware / 'platformio.ini').exists():
        raise ValueError('Not a VQEAF-OS firmware root')
    payload = archive.read_bytes() if archive else urllib.request.urlopen(URL, timeout=40).read()
    if hashlib.sha256(payload).hexdigest() != SHA256:
        raise ValueError('Lua 5.4.8 SHA256 mismatch: installation cancelled')
    destination = firmware / 'lib' / 'VqeafLua54' / 'src'
    destination.mkdir(parents=True, exist_ok=True)
    copied: list[str] = []
    with tarfile.open(fileobj=io.BytesIO(payload), mode='r:gz') as archive_file:
        for member in archive_file:
            if not member.isfile() or not member.name.startswith('lua-5.4.8/src/'):
                continue
            base = Path(member.name).name
            if not (base.endswith('.h') or base in CORE):
                continue
            if member.size > 512 * 1024:
                raise ValueError('Unexpected Lua source size')
            source = archive_file.extractfile(member)
            if source is None:
                raise ValueError('Missing archive file')
            (destination / base).write_bytes(source.read())
            copied.append(base)
    if not CORE.issubset(copied) or not {'lua.h', 'lauxlib.h', 'lualib.h'}.issubset(copied):
        raise ValueError('Missing Lua compiler source modules')
    license_data = None
    with tarfile.open(fileobj=io.BytesIO(payload), mode='r:gz') as archive_file:
        for member in archive_file:
            if member.isfile() and member.name in ('lua-5.4.8/COPYRIGHT', 'lua-5.4.8/LICENSE'):
                source = archive_file.extractfile(member)
                if source is not None:
                    license_data = source.read()
                break
    if license_data is None:
        license_data = _embedded_license((destination / 'lua.h').read_bytes())
    (destination.parent / 'UPSTREAM.txt').write_bytes(
        ('Lua 5.4.8 original source: ' + URL + '\nSHA256: ' + SHA256 +
         '\nLicense: MIT (see LICENSE.lua)\n').encode('utf-8'))
    (destination.parent / 'LICENSE.lua').write_bytes(license_data)
    (destination.parent / 'library.json').write_bytes(
        b'{"name":"VqeafLua54","version":"5.4.8","build":{"includeDir":"src"},"frameworks":"*","platforms":"*"}\n')
    print(f'Installed {len(copied)} verified Lua 5.4.8 upstream files into {destination}.')
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--firmware-root', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'firmware/VQEAF-OS')
    parser.add_argument('--archive', type=Path,
                        help='Offline official lua-5.4.8.tar.gz (same SHA256)')
    args = parser.parse_args()
    try:
        raise SystemExit(install(args.firmware_root, args.archive))
    except (OSError, ValueError) as exc:
        raise SystemExit('ERROR: ' + str(exc))
