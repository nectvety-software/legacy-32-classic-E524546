# VQEAF OS v2.5.0 — Host verification

Build: **PC simulation only**; no PlatformIO toolchain or actual SPI/SD/WiFi timings.

| Check | Result | Seconds |
|---|---|---:|
| `build_opaque_scanline` | PASS | 0.41 |
| `opaque_scanline_pixel_parity` | PASS | 0.01 |
| `build_perf_counter` | PASS | 0.05 |
| `perf_counter` | PASS | 0.00 |
| `build_historical_fallback` | PASS | 0.29 |
| `historical_fallback` | PASS | 0.00 |
| `build_compatibility_fallback` | PASS | 0.30 |
| `compatibility_fallback` | PASS | 0.00 |
| `ui_image_smoke` | PASS | 4.59 |
| `installer_reset_regression` | PASS | 5.68 |
| `board_gpio` | PASS | 0.60 |

**Overall:** PASS

**Graphics**: 144 RGB565 screen scenarios, 134,784 compared pixels, 30,846 legacy colored spans vs 4,320 scanline calls across these scenarios (host call counts, not FPS).
One display write transaction for each icon in the new opaque-only path, with byte-order setting restored.

**Not established:** on-device FPS, color accuracy on physical ST7789, image decoding latency, installer/theme hardware stability, battery usage.

## Other recorded regression checks in this development session

- `tools/verify_v242.py`: 17/17 host checks (theme runtime + Studio parser, HTTP chunk decoder, browser, QEAPP signature/installer and 33-C++-unit full host linker).
- `tools/verify_v243.py`: 5/5 host gates including 17/17 baseline, input gesture replay, and distinct production/demo signing keys. This check completed before the last visible version-string update; no installation or keypad paths changed afterwards.
- `tools/test_perf_diag_host.py`: 33/33 C++11 translation units and host linkage with `VQEAF_PERF_DIAG=1` enabled, tested after release version-string update.
- RGB565 scanline new renderer: ASan+UBSan 144/144 parity scenarios, no sanitizer findings in the observed run.

**These are C++ stub tests, not PlatformIO Xtensa compilation.** Reference clock, 40 MHz SPI throughput, display-byte swap, power/reset and installer/theme reliability must still be verified on the actual ESP32-S3.
