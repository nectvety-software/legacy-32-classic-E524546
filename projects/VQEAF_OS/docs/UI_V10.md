# Symbian S3 OS v1.0 — S60 Green UI

## Native geometry

- ST7789 portrait: **240 x 320**
- `Board::TFT_ROTATION = 0`
- Title/status bar: 27 px
- Content begins at Y=28
- Softkey navbar begins at Y=298 and is 22 px high
- Launcher: 3 columns x 4 rows, 78 x 66 px logical cells
- Procedural menu icon box: 36 x 36 px

The layout never switches to 320x240.

## S60 Green skin

The supplied Nokia/Symbian reference is reproduced as a lightweight procedural skin rather than a full-screen bitmap. The menu uses four fixed lime-green RGB565 bands, a dark green titlebar, pale-green softkey bar, black menu labels and a pale selected tile with white highlight. This saves RAM and allows each D-pad focus change to repaint only the old/new cells.

## Launcher

The twelve direct destinations are:

1. WiFi
2. Bluetooth
3. Music
4. File manager
5. Gallery
6. Internet / Qeafbrowser
7. Shell
8. Recovery
9. Settings
10. Notes
11. Apps
12. Library

D-pad wraps by row/column. START or SELECT opens the focused app. A/B returns to Idle.

## Themes

Settings > Theme cycles:

- S60 Green
- Classic beige
- Black

`Reset appearance` chooses S60 Green. A one-time `themeRev` migration changes v0.x installations to S60 Green once; later user selections remain persistent.

## Status icons

The titlebar intentionally renders only WiFi strength and a battery glyph. There is no SIM/cellular indicator because the E524546 board has no cellular modem. BLE and microSD status remain inside their respective applications.

## RAM behavior

The theme adds only constants and direct TFT primitives. It does not add a 240x320 framebuffer or retain a wallpaper bitmap in RAM. Existing bounded file/network shell tools and PSRAM-backed browser/image paths are unchanged.


## v1.0.1 title/statusbar alignment

The 27 px top bar is laid out as three equal 80 px zones on the 240 px panel. The clock is centered using `(SCREEN_W - textWidth(time))/2`; WiFi and battery occupy equal 40 px slots inside the right zone. This avoids overlap with long app titles and keeps the S60 chrome visually balanced.
