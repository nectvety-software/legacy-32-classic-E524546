# VQEAF OS v2.3.1 — Standardized system icon kit

- Added 12 independently-designed RGB565 system icons in exact 36×36 and 24×24 variants.
- Kept 36×36 for Home/Menu, introduced 24×24 for core system entries in list rows.
- Added flash-only nibble-RLE decoder plus theme-level focusFrame helper.
- Reused the existing `.vqeaf` palette and selection states. New icon art colors are currently fixed standard tokens; C++ palette overrides are supported at draw time but no new `.vqeaf` grammar is required.
- Preserved non-core procedural fallback and existing `.qeapp` 32×32 signed icon contract.
- Added reproducible PNG / exact SVG / RAW565 / sprite atlases, docs, standalone and integration tests.
- Host test result: C++11 `-Wall -Wextra -Werror` PASS, pixel parity 24/24 icons PASS, v2.3 Home/Menu partial redraw and v2.2 regression suites PASS.
- Limitation: PlatformIO build and ESP32-S3 board screenshots not run here.
