# VQEAF OS v2.3.3 — ESP32-S3 N16R8 / PlatformIO

Firmware baseline: **v2.3.2 Home/Menu + 12 RGB565 icons**; the OS UI, `.vqeaf` parser, and **signed** `.qeapp` installer are preserved. This revision concentrates on board/build reproducibility. **It is not yet hardware-validated.**

## 1. Board definition

`boards/vqeaf_s3_n16r8.json` is a project-local variant of PlatformIO `esp32-s3-devkitc-1` v6.10.0 board JSON. It identifies the target **ESP32-S3-WROOM-1 N16R8**, 16 MiB external QIO flash and 8 MiB OPI PSRAM. The variant/core/frequency remain `esp32s3` / Arduino / 240 MHz. The project settings select `board_build.arduino.memory_type = qio_opi`, **not** the stock N8/no-PSRAM defaults. N16R8 is a module configuration, not proof that every user-built carrier actually has those memory chips: the firmware prints measured sizes on boot.

Physical pin definitions remain solely in `include/BoardConfig.h`:

| Hardware | GPIO |
|---|---|
| ST7789 CLK/MOSI/CS/DC/RST/BL | 48 / 12 / 14 / 47 / 3 / 39 |
| MENU / UP / A / LEFT / START / RIGHT | 18 / 7 / 15 / 45 / 17 / 6 |
| OPTION / DOWN / B / SELECT | 8 / 46 / 5 / 16 |
| SDMMC 1-bit CLK / CMD / DAT0 / optional DAT3 | 13 / 11 / 9 / 10 |

Display: native **240 × 320 portrait**, `setRotation(0)`. SD: 1-bit SDMMC. Serial: 115200. Input active-low `INPUT_PULLUP`. **GPIO45 and GPIO46 are ESP32-S3 strapping pins**; holding LEFT/DOWN during power-on can affect boot. Do not alter their wiring or drive levels in software; assess the carrier on the real board if boot failures occur. Optional audio pins already present in the original code have not been redefined or verified here.

`src/core/BuildSanity.h` is a compile-time contract: ensures target ESP32-S3 (on actual ESP32 builds), requires `BOARD_HAS_PSRAM`, enforces screen size/orientation and detects duplicated documented pin assignments. The build flags for TFT_eSPI repeat the display pin mapping; `tools/check_board_config.py` detects any divergence between those flags and `BoardConfig.h`.

## 2. Flash layout

`partitions/vqeaf_16mb_ota.csv` vendors Arduino-ESP32 **2.0.17 `default_16MB.csv`** layout to avoid relying on a named file in a future toolchain:

| Partition | Offset | Size |
|---|---:|---:|
| NVS | `0x9000` | `0x5000` |
| OTA data | `0xE000` | `0x2000` |
| app0 / app1 | `0x10000` / `0x650000` | `0x640000` each (6.25 MiB) |
| spiffs (usable as LittleFS) | `0xC90000` | `0x360000` (3.375 MiB) |
| coredump | `0xFF0000` | `0x10000` |

`board_upload.maximum_size` is **6,553,600 bytes**, the actual size of **one app partition**, not the 16 MiB flash capacity. Upgrades from firmware with *another* partition table can erase/invalidate previous application and filesystem slots; back up SD and NVS settings before changing the partition table. `LittleFS` is provisioned as a partition but is **not auto-mounted by the existing storage service**. SD-backed themes/apps are unchanged. The coredump partition is reserved; no new automatic coredump reader was added.

## 3. Dependencies and toolchain

- PlatformIO Espressif32 **6.10.0** (Arduino-ESP32 **2.0.17** from that platform's published manifest).
- `bodmer/TFT_eSPI@2.5.43`
- `h2zero/NimBLE-Arduino@2.3.6`
- `bodmer/TJpg_Decoder@1.1.0`
- `bitbank2/PNGdec@1.1.6`

Version pins are to prevent unreviewed major upgrades, not a guarantee that all packages compile together. Confirm through a *real* PlatformIO compile. `lib_ldf_mode = deep+` resolves indirect library includes; `lib_compat_mode = strict` restricts architecture mismatches.

No new runtime library was added. `CONFIG_SPIRAM_USE_MALLOC` is no longer hardcoded in `build_flags` because it is an ESP-IDF configuration option, not a setting to override opportunistically from a project flag. The existing heap-capability allocations are unchanged.

## 4. Reproduce tests and build on Windows

Run these commands from the **`VQEAF-OS` project directory** (the folder containing `platformio.ini`):

```powershell
py -3 -m pip install --upgrade platformio pillow
py -3 tools/check_board_config.py
py -3 tools/test_build_sanity.py
py -3 tools/build_pio.py
# Alternative: powershell -ExecutionPolicy Bypass -File .\tools\build_pio_windows.ps1
```

`tools/build_pio.py` runs offline board checks, validates build guards, compiles/links **26 real firmware C++ translation units against native host stubs**, executes the **24 asset + Home/Menu C++ routing smoke**, then attempts `pio run -e vqeaf_os -v`. It logs the return codes and does **not** convert a missing PlatformIO tool into a PASS. On Windows, native `g++` is optional: if absent, host-only C++ checks are explicitly SKIPPED, while the actual PlatformIO target build still runs. For full host checks, install MinGW-w64/GCC and Pillow. For an extended (slower) legacy regression, run `py -3 tools/build_pio.py --full-host`.

Alternative direct commands:

```powershell
pio run -e vqeaf_os -v
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

If and only if the target build returns SUCCESS, its output should include `.pio/build/vqeaf_os/firmware.bin`, `.elf` and partition binaries. `build_reports/` contains preflight, host and target compilation logs; `build_reports/build_status.json` distinguishes their results.

Do **not** upload or switch partition layouts until you have backed up the device. Upload cannot be tested without the actual S3 device.

## 5. Boot checks on real hardware

1. Connect USB/serial at 115200 and reset. Verify `[VQEAF][BUILD]`, `[VQEAF][MEM]`, 16,777,216 flash bytes and approximately 8,388,608 PSRAM bytes. A warning indicates a detected mismatch; logs alone do not prove PSRAM bandwidth/reliability.
2. Confirm ST7789 renders **240×320 portrait** and both Home shortcuts and Menu's **12 icons** are present, with correct colors and key focus.
3. Check keypad 10 keys, especially SELECT long press (>600 ms), and that SDMMC initializes (do not hold strapping pin keys during power-on in the first test).
4. Repeat with SD removed, invalid `.vqeaf` and an unsigned `.qeapp` to test fallback and installer rejection; do not remove an active SD during writes.
5. Run existing `diag help` and relevant board diagnostics from Serial; capture the entire boot / error output and archive it with the PC `build_reports/` logs.

## 6. Continuous integration

`.github/workflows/platformio-build.yml` provides an *optional* GitHub Actions job for a repository whose **root is this `VQEAF-OS` directory**. It installs PlatformIO on the runner, runs both host verification and the real cross build, and uploads available firmware + logs. Merely including the workflow here does **not** mean a CI build has executed. If this firmware resides within a monorepo subfolder, adapt the workflow's `working-directory` and paths before use.

Reference board and partition definitions used for the vendored config:
- PlatformIO Espressif32 v6.10.0: `boards/esp32-s3-devkitc-1.json` (stock N8)
- Arduino-ESP32 2.0.17: `tools/partitions/default_16MB.csv`
