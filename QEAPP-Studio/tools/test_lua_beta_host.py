#!/usr/bin/env python3
"""Build a host-only beta firmware link with the bounded QEAPP runtime."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / 'firmware/VQEAF-OS'
STUB = FW / 'tools/host_stubs'
COMPAT = ROOT / 'runtime/host/compat'
VENDOR = FW / 'lib/VqeafLua54/src'
try:
    from tools.bootstrap_lua import CORE
    from tools.host_toolchain import find_host_tool
except ModuleNotFoundError:
    from bootstrap_lua import CORE
    from host_toolchain import find_host_tool

FLAGS = [
    '-std=c++11', '-fpermissive', '-DVQEAF_ENABLE_LUA=1', '-DQE_LUA_PSRAM_ALLOC=1',
    '-DARDUINO', '-DQEAPP_HOST_STUB_CRYPTO=1', '-DTFT_DC=47', '-DTFT_CS=14',
    '-DTFT_RST=3', '-I' + str(STUB), '-I' + str(COMPAT), '-I' + str(FW / 'include'),
    '-I' + str(FW / 'src'),
]


def run(command: list[str], env: dict[str, str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, capture_output=True, text=True, timeout=90, env=env)
    if result.returncode:
        detail = (result.stdout + '\n' + result.stderr)[-8000:]
        raise RuntimeError('Host beta build failed: ' + ' '.join(command) + '\n' + detail)
    return result


def main() -> int:
    cxx = find_host_tool('g++')
    cc = find_host_tool('gcc')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join((str(Path(cxx).parent), str(Path(cc).parent), env.get('PATH', '')))
    with tempfile.TemporaryDirectory(prefix='vqeaf-lua-beta-link-') as work:
        temporary = Path(work)
        objects: list[str] = []
        for index, source in enumerate(sorted((FW / 'src').rglob('*.cpp'))):
            output = temporary / ('fw-' + str(index) + '.o')
            run([cxx, *FLAGS, '-c', str(source), '-o', str(output)], env)
            objects.append(str(output))
        for index, name in enumerate(sorted(CORE)):
            source = VENDOR / name
            if not source.is_file():
                raise RuntimeError('Lua source is missing: ' + str(source))
            output = temporary / ('lua-' + str(index) + '.o')
            run([cc, '-std=c99', '-O2', '-I' + str(VENDOR), '-c', str(source), '-o', str(output)], env)
            objects.append(str(output))
        linked = temporary / 'linked'
        libraries = [str(STUB / 'host_globals.cpp'), str(STUB / 'host_entry.cpp')]
        if os.name == 'nt':
            libraries.insert(0, '-static')
        run([cxx, *FLAGS, *objects, *libraries, '-o', str(linked)], env)
        print(f'PASS beta: {len(objects)} translation units + bundled Lua host link')
        for beta in (False, True):
            executable = temporary / ('test_beta' if beta else 'test_stock')
            run([cxx, '-std=c++11', *(['-DVQEAF_ENABLE_LUA=1'] if beta else []),
                 '-I' + str(FW / 'src/services'), str(ROOT / 'runtime/host/tests/test_parser.cpp'),
                 str(FW / 'src/services/QeappFormat.cpp'), '-o', str(executable)], env)
            result = run([str(executable)], env)
            print(result.stdout.strip())
    print('NOTE: ESP32-S3 target build and PSRAM allocation are NOT verified')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as exc:
        raise SystemExit('ERROR: ' + str(exc))
