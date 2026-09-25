#!/usr/bin/env python3
"""Build the desktop host runner from the same bounded VM as ESP32 firmware."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

try:
    from tools.host_toolchain import find_host_tool
except ModuleNotFoundError:
    from host_toolchain import find_host_tool

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--system-lua', action='store_true')
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'build' / ('qe_lua_host.exe' if os.name == 'nt' else 'qe_lua_host'))
    args = parser.parse_args()
    if args.system_lua and os.name == 'nt':
        raise SystemExit('--system-lua is Linux-only')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    gcc_tool = find_host_tool('gcc')
    gxx_tool = find_host_tool('g++')
    build_env = os.environ.copy()
    tool_dirs = {str(Path(gcc_tool).parent), str(Path(gxx_tool).parent)}
    build_env['PATH'] = os.pathsep.join(sorted(tool_dirs)) + os.pathsep + build_env.get('PATH', '')
    base = ROOT / 'runtime'
    files = [base / 'src/QeLuaRuntime.cpp', base / 'host/qe_lua_host.cpp']
    options = [gxx_tool, '-std=c++17', '-O2', '-Wall', '-Wextra',
               '-I' + str(base / 'include')]
    if os.name == 'nt':
        options.append('-static')
    if args.system_lua:
        options += ['-I' + str(base / 'host/compat')]
        libraries = ['-Wl,-l:liblua5.4.so.0']
    else:
        vendor = ROOT / 'firmware/VQEAF-OS/lib/VqeafLua54/src'
        if not (vendor / 'lua.h').exists():
            raise SystemExit('Run python tools/bootstrap_lua.py first (official Lua 5.4.8)')
        options += ['-I' + str(vendor)]
        sources = [str(path) for path in vendor.glob('*.c')]
        sources = [path for path in sources
                   if Path(path).name not in ('lua.c', 'luac.c', 'onelua.c')]
        objects = []
        for source in sources:
            output = args.output.parent / (Path(source).name + '.o')
            subprocess.run([gcc_tool, '-std=c99', '-O2', '-I' + str(vendor),
                            '-c', source, '-o', str(output)], check=True, env=build_env)
            objects.append(str(output))
        libraries = objects + ['-lm']
        if sys.platform.startswith('linux'):
            libraries.append('-ldl')
    command = options + [str(path) for path in files] + libraries + ['-o', str(args.output)]
    subprocess.run(command, check=True, env=build_env)
    print('Host runner:', args.output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
