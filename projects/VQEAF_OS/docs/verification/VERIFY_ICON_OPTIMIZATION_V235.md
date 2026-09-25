# VQEAF OS v2.3.5 — measured icon optimization

| Metric | v2.3.4 | v2.3.5 |
|---|---:|---:|
| Sprite payload (bytes; excludes renderer/metadata) | 8,260 | 6,443 |
| Saved asset bytes | — | 1,817 (22.00%) |
| Variant sizes | 24×24, 36×36 | unchanged |
| Icon count | 12 × 2 | unchanged |
| Source header size (not flash) | 45,466 | 36,882 |

## Verification
The old production renderer and optimized production renderer are compiled independently against the same TFT RGB565 host framebuffer.
Exhaustive expected coverage: **179,712 pixel comparisons** across 192 scenarios (24 variants × 4 backgrounds × 2 clear modes).

| Test | Result |
|---|---|
| generate | PASS |
| parity_cpp | PASS |
| parity_source_png | PASS |
| home_menu_24_36_cpp | PASS |
| board_configuration | PASS |

**Overall:** PASS

**Not measured:** actual Xtensa firmware.bin Flash usage, on-device latency, SPI timing.
Do not interpret a host PASS as a target firmware build.

### Re-run
```powershell
py -3 tools\verify_icon_optimization_v235.py
pio run -e vqeaf_os  # separate target test on a configured ESP32-S3 development PC
```
