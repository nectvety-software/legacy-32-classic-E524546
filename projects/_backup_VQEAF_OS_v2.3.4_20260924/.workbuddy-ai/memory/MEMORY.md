# VQEAF OS — long-term project notes

ESP32-S3-WROOM-1 **N16R8**, PlatformIO + Arduino, ST7789 240x320 portrait.
Board on **COM3** = CH340 USB-UART. Verified chip: ESP32-S3 rev v0.2,
MAC `fc:01:2c:cc:eb:6c` (2026-09-24).

## Build / flash — working commands

```bash
pio run -e vqeaf_os                    # build
pio run -e vqeaf_os -t upload          # flash (460800 baud)
```

v2.3.4 `firmware.bin` = 1,473,136 B (22.48 % of the 6.25 MiB app slot).
App slot = `0x640000` = 6,553,600 B, matching `board_upload.maximum_size`.

## Packaging a flashable .img

```bash
python tools/make_flash_image.py            # -> dist/vqeaf_os_v2.3.4_*.img
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

## Serial capture (Windows, Git Bash)
pyserial lives in `~/.platformio/penv/Scripts/python.exe`.
Toggling DTR/RTS can raise
`GetOverlappedResult failed (PermissionError(13, 'Access is denied.'))` —
transient; retry the read in a loop.
