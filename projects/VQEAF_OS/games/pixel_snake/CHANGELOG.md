# Pixel Snake template v1.0.0
- Fixed-size game state; xorshift food RNG, safe body collision, one-turn-per-step.
- Pure C++11 5x7 bitmap glyphs and RGB565 pixel renderer (no PNG at runtime).
- 16x18 board on 240x320 portrait screen; 12x12 tiles; 3 palette presets.
- Signed QEAPP/2 config + custom 32x32 icon, pinned demo public key profile.
- High score saved in QeappDataService `state.bin`.
- Compiles host tests and actual signed installer verification on POSIX mock.
