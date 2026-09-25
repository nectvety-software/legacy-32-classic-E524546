# VQEAF OS — 12 system icon specification, v1.0

Target: ESP32-S3 N16R8 / ST7789 **portrait 240 × 320 RGB565**. This pack is original VQEAF artwork; it does not copy third-party UI sprites or logos. All icon PNG and C++ RLE bitmaps are generated from the same `tools/build_icons.py` shape definitions, not from screenshots.

## 1. Catalog and stable identities

| # | Canonical ID | Firmware legacy ID | Usage |
|---|---|---|---|
| 0 | `wifi` | `Wi` | WiFi scan/connection |
| 1 | `bluetooth` | `BLE`, `BT` | Bluetooth |
| 2 | `music` | `Mus` | Music |
| 3 | `files` | `Dir` | File manager |
| 4 | `gallery` | `Pic` | Gallery |
| 5 | `internet` | `Web` | Internet/Qeafbrowser |
| 6 | `shell` | `Term`, `Sh` | Shell terminal |
| 7 | `recovery` | `Rec` | Recovery |
| 8 | `settings` | `Set`, `Sys` | Settings |
| 9 | `themes` | `Th` | `.vqeaf` themes |
| 10 | `apps` | `App`, `All` | `.qeapp` applications |
| 11 | `library` | `Col` | Media library |

`UiIconCatalog::GRID_IDS` in the existing firmware already uses this exact order. Do not reorder the enum or assets without updating the mapping.

## 2. Stroke and pixel grid

| Rule | Menu variant | List variant |
|---|---:|---:|
| Canvas | 36 × 36 | 24 × 24 |
| Transparent safe inset (every edge) | ≥4 px | ≥3 px |
| Typical structure stroke | 2 px | 1 px |
| Fine detail / secondary contour | 1 px | 1 px |
| Minimum gap between distinct features | 2 px | 1 px |
| Rendering | Binary pixel edges, no alpha smoothing | Binary pixel edges, no alpha smoothing |
| Purpose | Home and Menu icons | Lists, settings, dialogs |

Glyphs have no baked background or selection border. All PNG files are transparent RGBA. The `raw565` optional files are width×height **little-endian** RGB565 with **magenta 0xF81F** as the transparent color key (they are *not* compatible with the signed `.qeapp` 32×32 icon section).

The exact per-pixel SVG variants (`assets/svg/{24,36}`) are generated from the same raster-index masks used by the C++ renderer, including transparent holes. Editable construction vectors are also provided under `assets/svg/geometry` but their browser antialiasing may differ from hardware pixel rendering. Do not use geometric SVG raster output for pixel screenshot tests.

## 3. Base palette and contrast

Token colors are for recognizable app identities and remain stable across `.vqeaf` themes by default. C++ accepts a caller-supplied `Palette` to replace any color **without recompiling RLE assets**. Standard identity swatches: cyan `#02CBED`, blue `#235FD9`, pink `#FE2996`, yellow `#F5EB42`, orange `#F7A620`, green `#16CD60`, silver `#D3E0E7`, ink `#23313A`. The role mapping and all RGB565 values are defined in `firmware/VqeafIconRenderer.cpp`.

**Focus is an OS widget state, not part of an icon.** First draw the selected tile background, then its 2 px focus outline, then the icon and label. The entire 24/36 icon bounding box is treated as the click target's visual only; hit testing uses the parent's cell. Always redraw *both* the old and newly selected tile when moving the D-Pad, not the full screen.

| Theme context | Focus background | Outer focus stroke | Inner stroke | Text on selected tile |
|---|---|---|---|---|
| Built-in VQEAF Lime | `0xDF93` | `0xFFFF` | `0xB6EE` | `0x0000` |
| AMOLED Red | `0x38A3` | `0xF9EB` (`colors.accent`) | `0x8967` (`colors.border`) | `0xFFFF` |
| VQEAF Night | `0x22B0` | `0x5698` (`colors.accent`) | `0x32AE` (`colors.border`) | light label |
| Imported `.vqeaf` | `keyPressed` → `colors.selected` | `accent` → `colors.accent` | `shellBorder` → `colors.border` | `keyText`/`selectedFg` |

In a custom theme, reject or repair focus colors with poor contrast against `selected` (aim for ≥3:1 contrast for focus and graphic boundaries, ≥4.5:1 for text). The existing v2.3 theme reader provides the palette: the icon renderer does **not** claim to parse new VQEAF DSL extensions itself. The OS keeps existing `.vqeaf` theme import and signed `.qeapp` behavior unchanged.

## 4. Portrait layout and integration points

* Home shortcuts: existing `VqeafLayout::homeShortcut(i)`, icon **36×36** in each 72×67 tile, `x=r.x+18`, `y=r.y+6`. Icon does not render its own tile.
* Menu: 3 columns × 4 rows, 78×66 grid cell, icon **36×36** centered, y=cell.y+3. The screenshot reference has icons at x≈21,99,177 and y≈31,97,163,229.
* Settings/app lists: **24×24** centered vertically inside each 42 px row; x=12, y=row.y+9. Secondary non-core icons retain the v2.3 36 px legacy renderer.
* In-page icons can use either size; never stretch a 24 px raster to 36 px at runtime.
* No icon font file is required. The existing v2.3 `UiVietnameseFont` remains in charge of Unicode captions and fallback ASCII is unchanged.
* Theme Studio `phoneShell` and `keypad` are virtual-frame components. They must NOT be drawn over the hardware LCD, though their palette tokens can style the OS.
* The `.qeapp` signed icon payload is **32×32 RGB565** with its existing cryptographic hashes and signature; this pack does **not** change that binary contract. Resizing an app icon must occur before signed-package creation by an authorized packager.

## 5. C++ rendering API

```cpp
#include "VqeafIconRenderer.h"

// Draw a selected 78x66 cell first:
tft.fillRect(cellX, cellY, 76, 66, theme.selected);
VqeafIcons::focusFrame(tft, cellX+1, cellY+1, 74, 64,
                       theme.accent, theme.border);
VqeafIcons::draw(tft, VqeafIcons::Id::WiFi,
                 cellX+20, cellY+3, 36,
                 theme.selected, VqeafIcons::Palette::standard());

// In a 42px-high list row:
VqeafIcons::draw(tft, VqeafIcons::Id::Themes,
                 12, rowY+9, 24,
                 theme.bg, VqeafIcons::Palette::standard());
```

The API checks IDs and sizes, validates RLE row spans and reads fixed arrays compiled into flash. It draws contiguous same-color horizontal runs using `TFT_eSPI::drawFastHLine`, skips transparent runs, and optionally clears only its own rectangular cell. There is no per-icon malloc, no PNG decode and no full-frame allocation. It does not change hardware GPIO, rotation, screen driver, network logic or security keys.

## 6. Validation and build

Standalone kit: `python tests/test_icon_pack.py` (needs Python Pillow and host g++). Integrated source: `python tools/test_ui_v23.py` and `python tools/test_vqeaf_icons.py`. Hardware PlatformIO build from the integrated project: `pio run -e vqeaf_os` (not run in this environment). Verify on actual board that focus moves between WiFi/Gallery, list rows do not clip glyphs, and no stale pixels remain after a tile redraw.

To regenerate, run `python tools/build_icons.py` in the standalone kit, and copy the 3 `firmware/VqeafIcon*` files into your project's `src/core` (or run integrated `python tools/rebuild_vqeaf_icons.py`). Do not add the standalone `tests/TFT_eSPI.h` mock to production `src`.
