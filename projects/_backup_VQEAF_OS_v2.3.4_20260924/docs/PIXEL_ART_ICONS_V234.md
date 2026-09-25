# VQEAF OS v2.3.4 — icon pixel art integration

## Implementation

- **Source:** the newly generated 12-icon image sheet, curated into transparent
  pixel assets by `tools/extract_pixel_icons.py`. Source reference included in
  `tools/pixel_icon_assets/reference/`.
- **Native assets:** 12 × 36x36 and 12 × 24x24 transparent PNGs in
  `tools/pixel_icon_assets/png`. The 24px set is optically resized from the
  36px variants, then saved as **native 24x24 assets**, not scaled on-device.
- **Runtime:** `src/core/VqeafIconData.h` generated, compiled into firmware
  as per-icon RGB565 palettes and bounded horizontal RLE runs.
- **Renderer:** `src/core/VqeafIconRenderer.cpp` reads local palette (not old
  global icon token colors), draws opaque runs with `drawFastHLine`, skips
  transparent index 0 and keeps the old `VqeafIcons::draw` C++ call signature.
- **UI:** Home (`SymbianUI::idleShortcutTile`), Menu
  (`SymbianUI::gridItem`), and list rows (`SymbianUI::listItem`) already use
  the common renderer; the new assets appear on all three routes automatically.
- **Focus/Theme:** focus outlines and tile backgrounds still read active
  `.vqeaf` theme. Icon illustration colors deliberately remain those in the
  source sheet for visual consistency across dark/light themes.
- **Apps:** installed `.qeapp` packages use their own existing 32x32 icon
  section. Signing/validation logic was not modified.

## Pixel safety + storage

| Setting | Value |
| --- | ---: |
| Screen | 240x320, portrait; ST7789 (unchanged) |
| Menu/Home icons | 36x36, inset >=4px transparent |
| Standard system-list icons | 24x24, inset >=3px transparent |
| Number of asset variants | 24 |
| RGB565 RLE+palette bytes | 8,260 bytes in `.rodata` |
| Runtime icon heap allocations | 0 |
| Allowed per-icon palette indices | 0 transparent + up to 31 colors |
| RLE run size | 1–8 horizontal pixels |

The bundled old v2.3.1 art is archived but not active. GPIO and display
rotation were not altered. Any effect on firmware's overall RAM/Flash size
requires a **real PlatformIO target build**, not a host simulator result.

## Developer workflow

```powershell
cd VQEAF-OS
py -3 -m pip install pillow
py -3 tools/rebuild_vqeaf_icons.py
py -3 tools/test_pixel_icons_v234.py
py -3 tools/test_board_host_smoke.py
py -3 tools/test_icon_integration_v234.py
py -3 tools/build_offline.py --dry-run --check-only
# On a machine with cached PlatformIO and the ESP32-S3 toolchain:
.\tools\build_offline.bat
# Then flash only after firmware.bin is created and checked:
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

`preview/v234_*_240x320.png` and `preview/v234_pixel_art_icon_catalog.png`
are renders and asset overviews, **not photographs of the ESP32-S3**.

## Limitations

The generated contact sheet is an illustration. Extracted source pixel art is
curated/pixelized rather than a byte-for-byte 36px asset supplied by the image
model. Complex per-key/phone-shell components from VQEAF Theme Studio are not
rendered by this firmware change. Physical ST7789 color order/contrast and
PlatformIO build still require testing on the real board.
