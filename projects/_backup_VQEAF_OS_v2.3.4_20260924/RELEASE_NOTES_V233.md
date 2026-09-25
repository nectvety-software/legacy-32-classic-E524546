# VQEAF OS v2.3.3 — PlatformIO N16R8 build configuration

**Source update (not a compiled `.bin`):** v2.3.2 -> v2.3.3.

- New `boards/vqeaf_s3_n16r8.json`, retaining physical pin mapping in `include/BoardConfig.h`.
- Explicit 16MiB flash partition table with two 6.25MiB OTA app slots and 3.375MiB flash filesystem area.
- Fixed `board_upload.maximum_size` to match an app slot instead of entire chip flash.
- Pinned PlatformIO / vendor library versions, QIO flash + OPI PSRAM setup, 240×320 ST7789 TFT flags.
- C++11 compile-time board/pin sanity guards and serial boot memory diagnostics.
- Self-check / logged build commands (`tools/build_pio.py`, `tools/build_pio_windows.ps1`); optional GitHub Actions workflow.
- Home/Menu 12 icons, `.vqeaf` theme loader, signed `.qeapp` trust verifier and original GPIO source unchanged.

## What was actually tested

Offline board/partition validation **PASS**; compile-time guard probes **PASS**;
26 C++ source files compiled and linked against host Arduino stubs **PASS**;
12 icon variants at both sizes and production Home/Menu C++ rendering routes **PASS**.

**ESP32-S3 PlatformIO firmware compile: NOT RUN/BLOCKED** in this environment
(`pio` absent; network-restricted environment cannot download it).
**NO `firmware.bin` GENERATED; NOT flashed/tested on physical hardware.**

Run `python tools/build_pio.py` on a development machine with PlatformIO.
Check `build_reports/platformio_build.log` after attempting a real cross compile.
See `docs/BUILD_REPORT_V233.md` and `docs/BOARD_BUILD_V233.md` for details.
