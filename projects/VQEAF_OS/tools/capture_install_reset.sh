#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
python3 -c 'import serial' || {
  printf '%s\n' 'Install dependency: python3 -m pip install -r tools/serial_requirements.txt' >&2
  exit 1
}
python3 tools/capture_install_reset.py --list-ports
printf 'Port [auto]: '
read -r PORT
PORT=${PORT:-auto}
exec python3 -u tools/capture_install_reset.py --port "$PORT" --out build_reports/device_serial
