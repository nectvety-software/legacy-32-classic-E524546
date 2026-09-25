# VQEAF OS v2.3.5 — Lossless Pixel-Art Icon Compression

Updated from the original **v2.3.4 Pixel Art Icons Full Source**. Firmware
includes the same 12 system icons at native 24×24 and 36×36. All original
24 PNGs and Home/Menu C++ layout are unchanged; only the compiled icon pack,
its decoder, reproducible tools and About/version metadata were updated.

Sprite payload (streams + RGB565 palettes): **8,260 → 6,443 bytes**,
saving **1,817 bytes (22.00%)**. This is not a measurement of total target
firmware Flash; ESP32-S3 PlatformIO binary build is still pending.

Actual host C++ tests: PASS. Independent old/new 179,712-pixel RGB565
framebuffer comparison: exact parity. New 240×320 Home/Menu host previews
also exactly match the v2.3.4 preview pixels.

Detailed Vietnamese guide: `docs/ICON_COMPRESSION_V235.md`.
One-command repeat: `python tools/verify_icon_optimization_v235.py`.
Reports and per-step logs: `build_reports/icons_v235`.

No changes to GPIO, board definition, .vqeaf theme parser, signed .qeapp
runtime, SD layout, WiFi/Bluetooth services, or original firmware apps.

Supplementary native PC linked-size measurement: 14,778 -> 13,408 bytes
(text+data+bss, same host harness, GCC -Os -flto). This is NOT an
ESP32-S3 Flash number. See `build_reports/icons_v235/native_linked_size.txt`.
