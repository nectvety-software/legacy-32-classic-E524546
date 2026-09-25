# VQEAF OS v2.3.4 — observed verification report

Verification performed with the checked-in C++ renderer and host TFT stub.

- `python tools/test_pixel_icons_v234.py`: **PASS** — 12 icon identities,
  2 sizes, safe transparent gutters, exact RGB565 comparison of 22,464
  framebuffer pixels against the compiled source palette and 24 icon PNGs.
- `python tools/test_ui_icon_contract_v232.py`: **PASS** — Home/Menu/list
  routing to the same production icon renderer; existing partial-redraw,
  theme and QEAPP identifiers retained.
- `python tools/test_board_host_smoke.py`: **PASS** — compiled actual
  `SymbianUI.cpp` and `VqeafIconRenderer.cpp` on host, checked screen routes.
- `python tools/check_board_config.py`: **PASS** — original 240x320 portrait
  panel, ESP32-S3 N16R8 memory config, 10 keys, SDMMC 1-bit pins and partitions.
- `python tools/test_ui_v23.py`: **PASS** — wider GUI/feature host regression.
  Historical screenshot pixels under 36px icon rectangles were excluded
  intentionally (art is changed); all 22,464 new icon pixels are tested
  separately. Reference palette/layout alignment outside changed icon artwork
  was Home 96.20% and Menu 96.19%, both above historical thresholds.
- `python tools/build_offline.py --dry-run --check-only`: **PASS**. Simulated
  build preflight, reports generated; **not** an Xtensa binary build.
- PlatformIO target build: **NOT RUN** (`pio` not installed in this environment).
- Physical ESP32-S3 test: **NOT RUN**.
- Firmware `.bin`: **NOT GENERATED**.

C++ icon asset payload in flash/rodata: **8,260 bytes** (data + RGB565 palettes,
excluding C++ table metadata). The firmware total size, SPI speed and RGB
color fidelity must be measured after an actual hardware build and flash.
