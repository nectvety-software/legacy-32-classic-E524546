# VQEAF OS — long-term project notes

ESP32-S3-WROOM-1 **N16R8**, PlatformIO + Arduino, ST7789 240x320 portrait.
Board on **COM3** = CH340 USB-UART. Verified chip: ESP32-S3 rev v0.2,
MAC `fc:01:2c:cc:eb:6c` (2026-09-24).

## Build / flash — working commands

```bash
pio run -e vqeaf_os                    # build
pio run -e vqeaf_os -t upload          # flash (460800 baud)
```

v2.4.0 `firmware.bin` = 1,482,720 B (22.6 % of the 6.25 MiB app slot),
sha256 `b3c9e82858880380a5d090c65fac8a2b196eea2a4a49ab0ae960514e197ad278`.
App slot = `0x640000` = 6,553,600 B, matching `board_upload.maximum_size`.
Clean build of v2.4.0 = 298 `Compiling` + 15 `Archiving`, ~321 s.
(v2.3.4 was 1,473,136 B / 295 Compiling.)

## Packaging a flashable .img

```bash
python tools/make_flash_image.py            # -> dist/vqeaf_os_v2.4.0_*.img
```

Merges bootloader `0x0` + partitions `0x8000` + boot_app0 `0xE000` + app
`0x10000`. Produces a sparse `_merged.img` (flash at 0x0) and a 16 MiB
`_16mb.img`. The script self-verifies; `boot_app0.bin` comes from the framework's
`tools/partitions/`, not `.pio/build`.

**Caveat:** flashing the merged image at 0x0 `0xFF`-fills the gaps, and NVS
(`0x9000`) sits in a gap — it erases saved settings. Fine for a factory image,
but say so before someone loses their config.

## Docs & evidence images

`docs/guide/HUONG_DAN_NAP_FIRMWARE.md` is the Vietnamese flashing guide. Its
images are rendered from **real captured logs** archived in `docs/guide/logs/`.

```bash
python tools/render_terminal_png.py <log> -o out.png --title "..."   # log -> PNG
python tools/make_flash_guide_images.py                              # refresh the guide
```

Never draw a plausible-looking log for documentation — capture a real run.
Strip sandbox noise (`[safe-delete]`, `sitecustomize.py` tracebacks) before
archiving: `grep -cE "sitecustomize|safe-delete|SystemExit|Traceback" logs/*.log`
must be 0. Always read rendered PNGs back to check for colliding labels.



## Hard-won constraints (do not re-litigate)

**1. `platformio.ini` carries a required LDF workaround.**
```ini
-D SOC_SDMMC_HOST_SUPPORTED=1
```
Without it the build dies with
`SD_MMC.h:21:10: fatal error: FS.h: No such file or directory`.
`SD_MMC.h` guards `#include "FS.h"` behind `#ifdef SOC_SDMMC_HOST_SUPPORTED`,
which lives in `soc/soc_caps.h` — a header the LDF preprocessor cannot expand.
Leave `lib_ldf_mode = deep+` and `lib_compat_mode = strict` alone; the vendor
libs (TFT_eSPI, PNGdec, NimBLE) need them.

**2. Never trust `[SUCCESS]`.**
If the log contains `Can not remove temporary directory .pio\build`, the build
did nothing against a stale `.sconsign*.dblite`. Delete `.pio/build` (keep
`.pio/libdeps`) and rebuild. A real build is ~300 s with 310 `Compiling` lines.

**3. `Serial` is the native USB CDC, not the UART port.**
`ARDUINO_USB_MODE=1` + `ARDUINO_USB_CDC_ON_BOOT=1` => `extern HWCDC Serial`
(native USB, GPIO19/20) and `extern HardwareSerial Serial0` (UART0).
All of `src/main.cpp` logs via `Serial`, so `[VQEAF][BUILD]`, `[VQEAF][MEM]`,
`SD: mounted`, `[S3DIAG][BOOT]` are **not** visible on COM3/CH340.
=> `docs/BOARD_BUILD_V233.md` §5 and `docs/BOARD_TEST_V22_VN.md` §2 are
misleading on this point; they only hold with the native USB port connected.
UART0 does still carry the ROM banner, core `ESP_LOGx` and panic dumps — use it
as a crash detector.

## Verification traps
- A 240x320 UI claim cannot be proven from serial alone; needs eyes on screen.
- Core NVS `NOT_FOUND` messages on first boot are normal (fresh NVS), not errors.
- **ROM banner `entry` is the BOOTLOADER's entry point, not the app's.**
  On v2.4.0: `bootloader.bin` / `merged.img` -> `403c98d0` (matches the banner),
  but `firmware.bin` -> `40377588`. Running `image_info` on `firmware.bin` and
  comparing to the banner looks like a mismatch and is a false alarm. Compare
  against `bootloader.bin`.
- The `.img` flash erase span `0x00000000-0x00179fff` straddles NVS at `0x9000`
  — that is the mechanism behind the "merged .img wipes settings" caveat.
- An SD `0x107` (`ESP_ERR_TIMEOUT` in `send_op_cond`) was seen on the first
  post-flash cold boot only, never again in 5 later boots. `StorageService::begin()`
  always calls `SD_MMC.begin()`, so *absence* of the message means the mount
  succeeded — treat this as card settling, not a firmware fault.

## Host test gate (`tools/verify_v240.py`) needs system tooling
7 host tests, run from the project root. They are host-only — they do NOT prove
an ESP32 build or a device flash; `pio run` + `-t upload` do that.
- Needs `g++` on PATH. On this machine it lives at `/c/msys64/mingw64/bin/g++.exe`
  (MSYS2, 16.1.0) and is **not** on PATH by default:
  `export PATH="/c/msys64/mingw64/bin:$PATH"`.
- `test_v15_signature.py` and `test_v24_app_manager.py` link `-lcrypto`, so they
  also need `mingw-w64-x86_64-openssl` from pacman.
- `test_v24_app_manager.py` called a bare `python3` (wrong on Windows: `python3`
  resolved to the managed Python 3.13, which has no `cryptography`). Patched to
  `sys.executable` — the penv interpreter has `cryptography` 50.0.1.
- Without the g++/openssl tooling the gate reports FAIL for environmental
  reasons only; read the per-test logs in `build_reports/v240/` before blaming
  the code (`FileNotFoundError: WinError 2` = missing `g++`).

## Serial capture (Windows, Git Bash)
pyserial lives in `~/.platformio/penv/Scripts/python.exe`.
Toggling DTR/RTS can raise
`GetOverlappedResult failed (PermissionError(13, 'Access is denied.'))` —
transient; retry the read in a loop.
