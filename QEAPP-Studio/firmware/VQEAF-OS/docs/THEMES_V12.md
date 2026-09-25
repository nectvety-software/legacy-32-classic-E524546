# Themes app — v1.2 / E524546 ESP32-S3

Target: ST7789 **240×320 portrait**. The titlebar clock remains centered;
only WiFi and battery glyphs appear in the compact status group.

## Installing a theme

1. Export a theme with VQEAF Theme Studio using **`@vqeaf 1.0`** syntax.
2. Copy the `.vqeaf` file into `/Themes/` on a **FAT microSD card**.
   You can copy the two bundled files from the project's `sd/Themes/` directory
   to try a known supported palette.
3. Turn off the device, insert the microSD, and power it on.
4. Open **Menu > Themes** (palette icon). Use Up/Down to choose a theme
   under the 4 built-in themes, press START/SELECT to apply immediately.
   OPTION opens Apply / Rescan microSD / Theme details. A/B returns Back.
5. Optional: File Manager > `/Themes` > select `.vqeaf` > Open takes you
   directly to the same Themes app with the file highlighted.

There is no need to compile firmware when adding or changing an SD theme.
Selection persists through restart via Preferences/NVS. The file itself
stays on SD; remove the card and next boot defaults visually to S60 Green
until the saved file can be opened again.

## Format accepted by ESP32 firmware

The importer intentionally reads palette colors only. Theme Studio's
`phoneShell`, `frameFx`, `keypad`, `networkLed`, badges, blend modes, glow
radius, arbitrary layers or images cannot be displayed by this firmware.
`glow` is used only as a flat warning/accent color. The original `.vqeaf`
can contain these other sections; the importer safely ignores them.

```text
@vqeaf 1.0
<theme id="my_s60_blue" name="Blue S60">
    palette {
        shellTop: "#17345F"
        shellBottom: "#102040"
        screen: "#172B4D"
        key: "#254365"
        keyPressed: "#426B91"
        keyBorder: "#A5B6C9"
        keyText: "#FFFFFF"
        subText: "#CDD5E0"
        accent: "#50C8FF"
        glow: "#FF7070"
    }
</theme>
```

Required keys: `screen`, `keyText`, `accent` (exact RGB hexadecimal
`#RRGGBB`). Optional keys map to panel, selection, border, title bar,
secondary text, popup and warning color. Missing optional values start
with built-in S60 Green defaults, overridden by the supplied colors. Unrecognized
palette keys are ignored so the importer can read newer Studio themes, while
malformed values for recognized display-color keys are rejected.

Files must be under 32 KiB and have <= 400 lines and <= 319 characters
per line; 16 `.vqeaf` files can appear in the list at once. `/Themes/` is
searched first, then the root and nested folders within a bounded depth.
Unsupported version/malformed file = an on-screen error, and the current
palette and saved preference remain unchanged.

## Interface and memory

- D-pad Up/Down: theme selection, partial redraw of only 2 rows.
- START / SELECT: validate & apply highlighted theme.
- OPTION: popup Apply / Rescan microSD / Theme details.
- A/B: close Themes and return to the previous screen.
- The selected filename is saved in NVS. The parser uses a 320-byte line
  buffer and fixed-size 16-entry file table; no 240×320 RGB565 framebuffer.
- SD file is read-only. The app does not run code from `.vqeaf` or change
  display GPIO, screen rotation, system paths or wireless behavior.

The bundled host test `python tools/test_v12.py` uses g++ when available
to exercise the **actual C++ parser**, not a Python imitation. Full target
build on ESP32-S3 still requires PlatformIO (`pio run`).

A 240×320 **layout preview** is provided in `preview/v12_themes_240x320.png`;
it is not a camera screenshot or evidence that the firmware has run on a board.
