# VQEAF OS v2.4.1 Pixel Snake — actual verification

Generated UTC: 2026-09-24T14:48:54.867360+00:00

## Deliverables

- Native C++11 game engine, renderer, strict signed-config parser, 32x32 signed icon.
- `.qeapp` demo package `snake_pixel` (QEAPP/2 text type), pinned only in optional `vqeaf_snake_demo` env.
- Default firmware publisher key untouched. Existing plain v2.4.0 opens this as text/denies untrusted demo package: native extension is required.
- Rendering target 240x320 portrait, 16x18 board, 12x12 tiles.

## Tests actually run

- PASS: GNU++11 deterministic gameplay, state bounds, food RNG, game-over, self-collision, wrap and parser negatives.
- PASS: Pure production C++ pixel renderer into 240x320 RGB565 framebuffer via host implementation.
- PASS: Genuine QEAPP/2 P-256 signature verifier and AppInstallerService on host POSIX SD mock using demo key.
- PASS: Tampered signed package rejected; changed installed payload rejected by catalog verification.
- PASS: Demo-signed package intentionally REJECTED by original production/default trust anchor.
- PASS: 33/33 firmware translation units compile and host link with Arduino/TFT stubs; not ESP32 target.
- PASS: Original v2.4.0 regression suite 7/7 (see below).
- NOT RUN: PlatformIO ESP32-S3 cross-build (environment lacks PlatformIO/toolchain).
- NOT RUN: flash and physical 240x320 LCD/keypad test.

## v2.4.0 regressions

- PASS: check_board_config.py (exit=0)
- PASS: test_v14_wiring.py (exit=0)
- PASS: test_v14_build.py (exit=0)
- PASS: test_v15_signature.py (exit=0)
- PASS: test_v24_app_manager.py (exit=0)
- PASS: test_pixel_icons_v234.py (exit=0)
- PASS: test_grid_nav.py (exit=0)

## Trust and scope

The `.qeapp` sample includes only signed **configuration and icon**; it cannot execute arbitrary Lua/ELF or Symbian SIS code. A dedicated native handler with matching `snake_pixel` ID is compiled into the patched firmware. Demo private key is not included and was discarded after generating this fixture. Before production create your own P-256 key outside the repository, pin its public header and re-sign the app.

## Build on a machine with PlatformIO

```powershell
cd VQEAF-OS
pio run -e vqeaf_snake_demo
pio run -e vqeaf_snake_demo -t upload
pio device monitor -b 115200
```

## Signed example

SHA-256 `fe21bdd34f4f580c24d5031f73d200c443b6609779bd22765cdaa57a482b8040`
