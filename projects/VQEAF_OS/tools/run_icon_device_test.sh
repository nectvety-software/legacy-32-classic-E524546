#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
python3 tools/verify_icon_device_v236.py
pio run -e vqeaf_icon_selftest
printf 'Build complete. Flash explicitly: pio run -e vqeaf_icon_selftest -t upload\n'
