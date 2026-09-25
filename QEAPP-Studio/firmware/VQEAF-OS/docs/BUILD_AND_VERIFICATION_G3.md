# VQEAF OS 2.1.0 — G3 source release and verification

The code in this package is a modified copy of the user's uploaded
`SymbianS3_OS_v2.0.1_board_test_kit(1).zip`, not a brand-new unrelated scaffold.
Original hardware GPIO and rotation 0 (240x320 portrait) are unchanged.
This is original Retro-Go-*inspired* tab/list/preview artwork and logic;
no Retro-Go GPL-2.0 source or bundled graphic assets were copied.

## Tests actually run in the preparation environment

`python tools/test_v21.py`: PASS. The test validates unchanged BoardConfig pinout,
boot to launcher, recovery, full GNU++11 mocked production source compilation
(23/23 units and full link), launcher 240x320 pixel geometry + item previews,
custom `.vqeaf` Theme Studio palette and RGBA colors, signed QEAPP/2 install
and tamper rejection, input/T9, WiFi state-machine, media, and board diagnostic
logic. This is HOST simulation using mock Arduino peripherals, not an MCU build.

`python tools/test_vqeaf_g3.py`: earlier standalone checks and full v2.0 regression
were run successfully during preparation. The current script also compiles the
launcher geometry test. Depending on compiler speed the full consolidated suite
can take several minutes; `test_v21.py` is the recommended release gate.

## What is not verified

PlatformIO was NOT installed in the preparation environment. No target BIN was
built, no flash/upload was attempted, and no ESP32-S3, LCD, BLE/WiFi radio,
real SD card, NTP clock or real-time FPS measurement was available.

## Build and run

```powershell
cd VQEAF-OS
python tools/test_v21.py
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

Copy `.vqeaf` files to `/System/Themes/` on FAT microSD. The bundled
`vqeaf_night.vqeaf` / `vqeaf_day.vqeaf` are known parser samples.
See `docs/VQEAF_VERIFY_V21.md` for manual board tests and signing instructions.

## Limitations

The launcher itself is fully replaced; most built-in *inner app pages* remain
from v2.0.1, recolored by the selected theme, awaiting separate layout work.
The firmware's `.vqeaf` reader uses colors, not the Android virtual phone
frame/keypad animation and embedded image effects. Installed `.qeapp` files
must follow the original cryptographically signed QEAPP/2 web/text format:
this release does not execute arbitrary Lua or native binary app payloads.
