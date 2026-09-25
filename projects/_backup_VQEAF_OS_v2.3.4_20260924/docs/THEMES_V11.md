# Symbian S3 OS v1.1 themes

Target: ESP32-S3 N16R8, ST7789 portrait 240x320.

## Built-in themes

1. S60 Green (default)
2. AMOLED Red
3. Black
4. Classic beige

AMOLED Red is adapted from `VQEAF-Theme-Studio/themes/amoled_red.vqeaf`. The original source palette maps to the embedded renderer as follows:

- `screen #000000` -> main background
- `key #171717` -> panels / softkey surface
- `keyPressed #38161B` -> selected tile/list row
- `keyBorder #8A2E3B` -> borders
- `keyText #FFFFFF` -> primary text
- `subText #B78E94` -> secondary text
- `shellTop #141414` -> titlebar
- `accent #FF3D5B` -> focus/underline/status decoration
- `glow #FF2D4D` -> danger/glow accent

The firmware compiles those colors to RGB565 constants. It does not parse the `.vqeaf` files at runtime; this avoids heap use, file I/O during painting and theme-induced flicker. The files under `themes/` are the editable source/reference for future Theme Studio export/import support.

## Statusbar geometry

The clock remains centered at x=120. The right edge contains only WiFi and battery because this device has no SIM/cellular modem. Each glyph is 11 px wide, the gap is 6 px, and the group has a 6 px right margin. This places WiFi at x=206 and battery at x=223 on the 240 px screen.
