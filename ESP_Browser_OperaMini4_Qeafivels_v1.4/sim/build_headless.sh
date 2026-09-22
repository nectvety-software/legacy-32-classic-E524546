#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
g++ -O2 -std=gnu++17 -Isim/shims -Isim -Iinclude \
  -o sim/espbrowser_headless \
  sim/headless_main.cpp sim/sim_arduino.cpp \
  src/main.cpp src/wml.cpp src/http.cpp src/store.cpp
./sim/espbrowser_headless
