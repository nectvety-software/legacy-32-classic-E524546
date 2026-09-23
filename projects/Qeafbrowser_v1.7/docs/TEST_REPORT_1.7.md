# Qeafbrowser v1.7 — Overview/Thumbnail Cache Regression

## Scope

- Opera Mini 4-style overview cursor refinement.
- PSRAM LRU thumbnail cache and persistent LittleFS RGB565 cache.
- Preview page-tile overview based on rendered pixel heights.

## Results

- Keypad focus regression: PASS.
- Qeafivels HTTPS redirect/title parsing: PASS.
- JPEG and PNG thumbnails: PASS in simulator path.
- Cold thumbnail cache: decoded thumbnails written as LittleFS cache v2 with CRC32.
- Warm thumbnail cache: v2 files loaded from LittleFS and promoted to PSRAM.
- PSRAM cache overflow gate: 8 unique thumbnail keys forced through 6 RAM slots; LRU eviction observed and evicted entry reloaded from LittleFS. PASS.
- Overview zoom: x1..x8, D-Pad chunk pan and OK-to-enter selected area: PASS.
- Preview mosaic: page tiles split by actual rendered line heights and reuse cached image thumbnails. PASS.

## Hardware note

The source is wired for ESP32-S3 `TJpg_Decoder` and `PNGdec` through PlatformIO dependencies. This environment does not contain the ESP32 PlatformIO toolchain or physical E524546 hardware, so the firmware branch was not flashed here. The Linux simulator regression compiles and runs the shared parser/UI/cache/navigation sources.
