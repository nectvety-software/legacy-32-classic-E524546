# VQEAF OS v2.3.6 — icon verification / whole-firmware size instrumentation

- Diagnostic `vqeaf_icon_selftest`: 192 golden RGB565 CRC32 image cases from
  frozen v2.3.4 C++ production renderer, with CPU decode timing, actual TFT
  draw timing and internal/PSRAM memory before/after.
- Renderer refactored so production TFT and device test share exactly the same
  span decoding function; host proves v2.3.4 visual parity after refactor.
- Two FULL production firmware PlatformIO environments: only icon baseline
  vs optimized codec changes. Logs both `firmware.bin` sizes and ELF sections.
- In this release environment, PlatformIO/toolchain are unavailable. Target
  firmware.bin size and actual ESP32 serial capture: **NOT MEASURED YET**.
- Host x86 linked harness (NOT full firmware): 14,778 B → 13,444 B (−1,334 B).
- Static icon sprite payload: 8,260 B → 6,443 B (−1,817 B, −22.00%).
- See `docs/ICON_DEVICE_AND_FLASH_MEASUREMENT_V236.md` for commands and caveats.
