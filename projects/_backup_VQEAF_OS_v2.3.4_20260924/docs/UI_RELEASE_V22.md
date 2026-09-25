# VQEAF OS v2.2 — Firmware UI implementation notes

## Inherited source and what was changed

This is a **source-level continuation** of the user's VQEAF v2.1 source built from `SymbianS3_OS_v2.0.1_board_test_kit`. It is not an empty scaffold and it does not replace the browser, crypto or WiFi services. All board pins and the baseline `platformio.ini` remain intact.

| Layer | File | Change |
|---|---|---|
| Hardware | `include/BoardConfig.h`, `platformio.ini` | **No changes to GPIO** or build framework; portrait 240×320 |
| Public branding | `src/main.cpp` | VQEAF OS v2.2 splash/about; boot into Home, not optional Explorer |
| Main navigation | `src/apps/LauncherGrid.cpp` | Original 3×4 Menu logic restored as the **real** main Menu; route via MENU |
| Explorer | `src/apps/Apps.cpp`, `src/apps/Apps.h`, `src/launcher/` | Previous six-tab view separated into `ExplorerApp`, reachable from Menu Options |
| Screen identity | `src/core/Types.h`, `src/main.cpp` | `ScreenId::Explorer` and safe routing without renumbering SD/NVS payload identifiers (these enum values are runtime-internal) |
| Geometry | `src/core/UiLayoutGeometry.h`, `src/core/SymbianUI.h` | Share measured 27 px header, 28..297 content, 298..319 footer; 3×4 grid and six list rows |
| Palette | `src/core/SymbianUI.cpp` | Apply external `.vqeaf launcher{}` to existing LCD Home/Menu/app chrome; selected ink/softkeys, no fake charge level |
| Pixel atlas | `src/core/UiScreenAtlas.h/.cpp`, `docs/pixel_atlas.json` | 16 screens, 162 bounded target rectangles, generated from script |
| Documentation | `docs/UI_PIXEL_ATLAS_V22.md`, `tools/gen_pixel_atlas.py` | Fine-grained screen placements including alternate states, dialogs and hit zones |
| Security | `src/services/{QeappFormat,QeappSignature,AppInstallerService}.*` | **Retained unchanged**, signed QEAPP/2 only |

## Component tree

```text
platformio.ini (existing Arduino/TFT_eSPI)
  src/main.cpp  (board bootstrap + service loop + event router)
    include/BoardConfig.h (unchanged pins)
    core/InputManager.* (unchanged Game/T9/hold SELECT)
    core/UiLayoutGeometry.h (compile-time shared geometry)
    core/UiScreenAtlas.* (generated target metadata for 16 screens)
    core/SymbianUI.* / alias VqeafUI (existing screen drawing facade)
    services/ThemeFileService.* (Studio 1.0 palette & optional launcher overrides)
    apps/LauncherGrid.cpp (Home screenshot's 3x4 Menu entry)
    apps/Apps.cpp (built-in apps + optional ExplorerApp)
    launcher/LauncherView.* (optional tab/list Retro-inspired Explorer)
    services/AppInstallerService.* + QeappSignature.* (QEAPP/2 verified apps)
```

## Implementation scope vs pixel target

- **Implemented routing & drawing**: Home first, 3×4 grid focus redraw, original 16 built-in screen draw handlers, theme-colored global chrome/selection, optional Explorer. **All main service objects are real inherited implementations**, not blank placeholders.
- **Measured from baseline C++/reference**: header 27, footer 22, grid 3×4 (78×66), clock/network Home cards, home quick icons, six row lists (42).
- **Target-only positions**: some gallery thumbnail-grid, fully reflowed music/shell/browser, pixel-perfect icon/text boxes in secondary screens. These are catalog entries and preview blueprints **not yet necessarily bound** to all legacy draw callsites. An explicit later migration can replace per-screen magic constants gradually and use `UiScreenAtlas::get()`.
- **Theme limits**: current parser is a bounded, static reader of Studio `.vqeaf` **color fields only**. Resources, gradients, per-virtual-key image styles, animation and web Studio-only frameFX aren't executed by the physical firmware. Studio exporting and reimporting may not preserve optional OS `launcher{}` extensions unless its serializer is updated separately.
- **Text limits**: baseline TFT_eSPI bitmap fonts are not a complete UTF-8 Vietnamese glyph solution. Include a separately licensed font/glyph raster module before asserting full Vietnamese UI.
- **UI behavior limits**: tabbed Explorer lives as an optional subview and is not intended to turn the 3×4 main Menu into a tab browser. Some existing old app help strings/styling remain pending.

## Acceptance tests on actual ESP32-S3

1. Back up the original SD and keep original `.bin` for rollback; verify hardware wiring in supplied `docs/system_prompt_phan_cung.md` / DESIGN. Check the board `platformio.ini` N16R8 settings before flashing.
2. `pio run -e vqeaf_os` **SUCCESS required**. Then `pio run -e vqeaf_os -t upload` and `pio device monitor -b 115200` with device attached.
3. Normal boot displays Home General (clock card, WiFi card, WiFi/Music/Files three shortcuts). MENU opens 3×4 Menu; focus travels between exactly 12 entries and updates only affected tiles; MENU/A from grid returns Home.
4. MENU from each system app returns grid, OPTION menu in grid can open optional six-tab Explorer, A in Explorer returns grid (see implementation). Browser/WiFi hot-switch shouldn't reset radio. SELECT hold switches Game/T9 without unintended START.
5. Try `vqeaf_reference_lime.vqeaf`, `vqeaf_night.vqeaf`, and a fresh Studio-generated palette: Home, Menu, app header/selected labels/footer visibly change. Remove SD while theme active, simulate malformed/oversized theme and boot Recovery by holding DOWN.
6. Install a signed `.qeapp` from the publisher key configured in your own firmware; test tampered, unsigned and authorized packages. The existing QEAPP binary ABI, trust key and receipt persistence must not change.
7. Capture 16 app/screens both offline and populated states and compare `docs/UI_PIXEL_ATLAS_V22.md`; record actual FPS/heap/PSRAM, restore time and any byte ordering mistakes.

### Verification evidence

`python tools/test_ui_v22.py` is the reproducible host gate. It compiles and links the **25 actual C++ translation units** with Arduino peripheral stubs, executes target-geometry assertions (16 screens, 162 regions), the actual Theme Studio `.vqeaf` parser, button/T9 test, Explorer widget raster bounds, ECDSA package tamper gate, WiFi and hardware diagnostic unit tests.

**Not tested here:** PlatformIO / ESP32-S3 compiler and libraries, DMA SPI, physical LCD, SD removal, WiFi signal/TLS in the field, key bounce and speed. `preview/*.png` are **design blueprints only**.
