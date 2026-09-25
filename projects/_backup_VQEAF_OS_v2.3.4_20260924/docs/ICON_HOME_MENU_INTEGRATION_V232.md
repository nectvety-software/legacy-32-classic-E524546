# VQEAF OS v2.3.2 — 12 standard icons on real Home/Menu C++ GUI

**Source:** v2.3.1 standardized VQEAF icon pack, installed into the existing
`SymbianUI.cpp` and `LauncherGrid.cpp` paths. No Retro-Go images/source copied.

## Exact rendering contract (LCD portrait 240×320)

- Home slots 0–2 are **WiFi / Music / Files**. Tile `i` is `(8+76i,191,72,67)`;
  the independent 36×36 RGB565 icon begins at `(tile.x+18,197)`.
- Menu: WiFi/Bluetooth/Music; Files/Gallery/Internet; Shell/Recovery/Settings;
  Themes/Apps/Library. Cell `(col,row)` is `(1+78col,28+66row,76,66)`;
  each 36×36 icon begins at `(21+78col,31+66row)`.
- `UiIconCatalog.h` now owns the typed icon arrays. The visible names may be
  localized without silently switching bitmap identity.
- System lists use 24×24 independently rasterized versions at `(12,row.y+9)`.
  Signed external `.qeapp` application icons remain their own 32×32 payload.
- In every location transparent pixels show the underlying tile color;
  the RLE icon decoder needs no icon-sized SRAM or PSRAM scratch bitmap.

## Theme and focus

- No `.vqeaf` grammar changes. Existing launcher selection, border, title and
  panel palette values continue to color the tile and focus frame.
- Standard icon artwork is fixed RGB565 by default and can receive an explicit
  `Palette` per render call in future; icon artwork itself does not override
  the theme's selected tile color. Home selection and Menu selection do not
  redraw whole LCD on D-pad movement (two modified tiles only).
- Icons have 4px safe edge in the 36px version and 3px in 24px version.

## Regression workflow

```
python tools/test_icon_integration_v232.py
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

The Python command compiles the *real* `src/core/SymbianUI.cpp` and
`src/core/VqeafIconRenderer.cpp` with a TFT host shim; it verifies every pixel
of 12 main Menu icons, 3 Home icons, and 12 small list variants against the
standalone renderer (not a copy of the images). It also checks two-tile focus
redraw, built-in and imported `.vqeaf` custom focus coloration, v2.3 Home/Menu screenshot-mask checks, older
UI/firmware services, and produces **four native 240×320 PNG previews**.
The host TFT shim uses an approximation of hardware font rasterization;
preview PNGs are *not* actual ESP32 screenshots.

PlatformIO target compilation, ST7789 panel timing, heap/FPS and physical
WiFi/SD/BLE checks require a connected ESP32-S3 board. Do not equate a host
C++ test with verification of a flashable firmware binary.
