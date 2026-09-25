# VQEAF OS v2.3.2 — 12 icon integration (240×320 portrait)

This is the full **editable PlatformIO/Arduino C++ firmware source** based on
v2.3.1 (original handheld board pin assignments unchanged). The implementation
retains the screenshot-style **Home dashboard** and **3-column × 4-row Menu**.
It does not replace them with the optional Retro-inspired Explorer.

## Source edits

- `src/core/UiIconCatalog.h`: typed ID maps for all 12 Menu cells and Home's
  three pinned shortcuts (WiFi, Music, Files). The same asset is used in both
  contexts irrespective of translated text.
- `src/core/SymbianUI.cpp`: actual Home/Menu C++ raster calls for the separate
  **36×36** RGB565 RLE assets; **24×24** for list entries; existing non-system
  procedural fallback kept. Selected colors, highlight borders and app chrome
  remain controlled by theme `.vqeaf`.
- `src/core/VqeafIconRenderer.{h,cpp}` and `VqeafIconData.h`: identical flash
  assets to icon kit v1 (~3,980 total bytes of icon RLE), no dynamic allocations.
- `src/apps/LauncherGrid.cpp`: production main 12-item Menu uses semantic
  `UiIconCatalog::GRID_IDS`, changing selection redraws only two old/new cells.
- `src/main.cpp`: Home shortcut navigation updates only two tiles.
- `tools/vqeaf_host/test_icon_routes.cpp`: compiles the **actual production
  SymbianUI.cpp** against a framebuffer TFT host double and verifies exact
  icon pixels, three Home and twelve Menu placements, 24px lists, partial
  updates, builtin/external focus colors. This is **not an ESP32 emulator**.

## Exact-size screenshot previews (from actual C++ GUI code on the host)

- `preview/v232_home_240x320.png`
- `preview/v232_menu_240x320.png`
- `preview/v232_home_selected_240x320.png`
- `preview/v232_menu_selected_240x320.png`
- `preview/v232_home_menu_contact_sheet.png` (2×2 convenience sheet)

### Development environment tests

`python tools/test_icon_integration_v232.py` requires Python, Pillow and `g++`
(or `tools/verify_icons_v232.bat` on Windows, with Python in PATH). It runs
24-icon asset parity, actual GUI C++ host pixel validation and full v2.3/v2.2
existing regressions, including WiFi profiles, signed QEAPP and UI geometry.

Target build and flash **not performed**; on a machine with PlatformIO:

```powershell
cd VQEAF-OS
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

Device spot-check: verify all 12 Menu cells and Home's WiFi/Music/Files at
native size and inspect selected/unselected transitions for dirty artifacts;
repeat with builtin Lime then SD-loaded `.vqeaf`. Test SD missing, malformed
`.vqeaf` and installed signed `.qeapp`. A host PNG cannot prove panel color
order, backlight or DMA behavior on physical hardware.

**No new pins, no bitmap licensing from Retro-Go, no change to `.vqeaf` grammar
or signed QEAPP/2 contract.**
