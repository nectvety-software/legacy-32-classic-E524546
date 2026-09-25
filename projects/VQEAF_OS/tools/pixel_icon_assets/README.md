# VQEAF OS v2.3.4 — Pixel art system icons

This is the **active source** for the 12 VQEAF OS icons; `tools/icon_assets` is
preserved only as the legacy v2.3.1 sprite kit. The reference image came from
user-requested image generation in the current conversation. The `png/` folder
contains exactly 12 transparent 36x36 RGBA icons and 12 24x24 derivatives.

**Regenerate firmware RLE (Python + Pillow only on host):**

```shell
python tools/pixel_icon_assets/compile_pixel_icons.py
python tools/test_pixel_icons_v234.py
python tools/test_board_host_smoke.py
```

**Optional: revise individual icon art from the contact sheet:**

```shell
python tools/extract_pixel_icons.py tools/pixel_icon_assets/reference/original_generated_contact_sheet.png --out tools/pixel_icon_assets/png
python tools/pixel_icon_assets/make_24_from_36.py
python tools/pixel_icon_assets/compile_pixel_icons.py
```

Do not invoke the archived `tools/icon_assets/tools/build_icons.py` to update
this release; that would restore the previous art. Use the transparent PNGs.

### Runtime format
- Exactly 24x24 or 36x36; no runtime scaling, no external image decoder.
- Palette index 0: transparent, allowing backgrounds from active `.vqeaf`.
- 31 or fewer nontransparent RGB565 colors **per asset**.
- Each row holds 1-byte runs: colorIndex (high 5 bits), runLength-1 (low 3).
- Full icon payload is stored as C++ `static const` in `VqeafIconData.h`.
- The renderer emits runs with TFT_eSPI `drawFastHLine` (no heap/PSRAM allocation).
- Theme changes menu tile background and focus; baked pixel-art hues are fixed.
- Third-party signed `.qeapp` packages keep their own signed 32x32 icon payloads.

### What changed
Menu 3x4, Home WiFi/Music/Files, and all 12 associated list rows now use the
new art via existing `VqeafIcons::draw` routes; all other OS functions, pin
assignments, SD, browser, and app signatures are unchanged.
