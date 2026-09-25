# VQEAF OS v2.3.3 — Build report (2026-09-24 UTC)

**Base:** user-provided VQEAF OS v2.3.2 full-source ZIP. SHA-256: `f7f8bb0909bf9dcd5290852a76bb8d76bdf53ccec15c1c726e3906478533bc50`.

## Verified results in THIS environment

| Check | Result | Evidence |
|---|---|---|
| Offline custom board / memory / pin / partition sanity | **PASS** | `build_reports/preflight.log` |
| C++ compile-time guards (valid config and two deliberate failures) | **PASS** | `build_reports/build_guards.log` |
| Host C++11 compile/link (all 26 original firmware `.cpp` files, mock ESP32 APIs) | **PASS** | `build_reports/host_cpp_link.log` |
| 12 icons × 24/36 px, RLE checks, real Home/Menu GUI C++ host smoke | **PASS** | `build_reports/host_icon_ui.log` |
| Full legacy/regression integration suite | **NOT RUN** | Optional `python tools/build_pio.py --full-host` |
| **ESP32-S3 PlatformIO target compilation** | **BLOCKED / NOT RUN** | `build_reports/platformio_build.log`, `toolchain_availability.log` |
| Binary `firmware.bin`, device flash test | **NOT GENERATED / NOT RUN** | PlatformIO not available; no device connected |

`python tools/build_pio.py` exited **2** due specifically to PlatformIO being unavailable, **not** because the ESP32 firmware compiled and failed. Running `pio run -e vqeaf_os` directly produced **`pio: command not found` (exit 127)**. A `pip install platformio` attempt was also unable to obtain the package because the sandbox had no working package-network access. **Do not report a PlatformIO SUCCESS until a local or CI build succeeds and produces an actual `firmware.bin`.**

### Build status JSON

```json
{
  "timestamp_utc": "2026-09-24T12:00:29.455532+00:00",
  "board": "vqeaf_s3_n16r8",
  "environment": "vqeaf_os",
  "offline_preflight": "PASS",
  "build_guards": "PASS",
  "host_cpp_link": "PASS",
  "host_icon_ui": "PASS",
  "host_full_regression": "NOT_RUN",
  "platformio_target": "BLOCKED_MISSING_PIO",
  "firmware_bin": "NOT_GENERATED"
}
```

## Changed files

- `platformio.ini`: exact platform/library pins, project-local N16R8 board, correct one-slot binary limit, QIO/OPI memory flags, consistent TFT_eSPI settings.
- `boards/vqeaf_s3_n16r8.json`: explicit ESP32-S3/WROOM-1 N16R8 board definition derived from PlatformIO's documented S3-DevKitC-1 build properties, upgraded from 8 MiB/no PSRAM defaults.
- `partitions/vqeaf_16mb_ota.csv`: explicit Arduino 2.0.17 16 MiB OTA partition layout.
- `src/core/BuildSanity.h`: compile-time target and wiring guards.
- `src/main.cpp`: boot-time serial diagnostics for detected flash, PSRAM and free heap; displayed version 2.3.3.
- `src/services/ShellService.cpp`: version report 2.3.3 (storage settings namespace untouched to preserve saved preferences).
- `tools/check_board_config.py`, `tools/test_build_sanity.py`, `tools/test_board_host_smoke.py`, `tools/build_pio.py`, `tools/build_pio_windows.ps1`: reproducible checks with status/error logs.
- `.github/workflows/platformio-build.yml`: optional **not-yet-executed** cross-build CI.
- `docs/BOARD_BUILD_V233.md`, `docs/BUILD_REPORT_V233.md`: build/recovery instructions and observed outcomes.

**Verified unchanged against v2.3.2:** `include/BoardConfig.h`, `src/core/VqeafIconData.h`, `src/core/SymbianUI.cpp`, `src/apps/LauncherGrid.cpp`, `src/services/QeappSignature.cpp`, `src/services/ThemeFileService.cpp`. Therefore LCD/SD/keypad physical mappings, production icon assets, `.vqeaf` loader and signed QEAPP verifier remain source-identical. This is not a claim of MCU binary equivalence.

## Required next verification on an internet-enabled PC / CI

```powershell
cd VQEAF-OS
py -3 -m pip install --upgrade platformio pillow
py -3 tools/build_pio.py
# After PASS only:
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

If the cross-build produces an error, send **`build_reports/platformio_build.log`** so we can correct the exact SDK/library compiler/linker failure. Check boot flash/PSRAM and SD/keys on the actual ESP32-S3 separately before declaring hardware compatibility.
