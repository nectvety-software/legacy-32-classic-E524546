#!/bin/sh
# Supports --dry-run, --simulate-failure, --buildfs, --host-tests.
set -eu
cd "$(dirname "$0")/.."
if command -v python3 >/dev/null 2>&1; then
  exec python3 tools/build_offline.py "$@"
fi
if command -v python >/dev/null 2>&1; then
  exec python tools/build_offline.py "$@"
fi
echo "[ERROR] Python 3 not found" >&2
exit 2
