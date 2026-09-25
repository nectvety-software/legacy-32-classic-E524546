# VQEAF OS v2.3 — Home/Menu pixel integration (240×320 portrait)

**Tình trạng:** implemented into the original C++ firmware. The color/layout comparison here uses host rasterization of `src/core/SymbianUI.cpp` with a simulated TFT, **not a photograph or a verified flashable `.bin`**. The 16-screen v2.2 atlas remains the overall target; this pass hardens the two reference-critical Home/Menu screens plus shared icons/fonts. Other app bodies may still contain v2.2 legacy internal spacing.

## 1. Non-negotiable hardware and ABI

- ESP32-S3 N16R8, TFT ST7789 native **240×320 rotation 0** (TFT_eSPI), 8 MB PSRAM, SDMMC 1-bit.
- `include/BoardConfig.h` pinmap untouched: LCD CLK48, MOSI12, CS14, DC47, RST3, BL39; buttons MENU18 UP7 A15 LEFT45 START17 RIGHT6 OPTION8 DOWN46 B5 SELECT16; SD CLK13 CMD11 DAT0 9 (DAT3/CD10 optional).
- Do not replace the existing QEAPP/2 installer with a renamed `.zip` or arbitrary Lua executor. Its package payload hashing, ECDSA-P256 signature, trusted publisher-key check and launch receipt remain in place.
- `.vqeaf` is the original VQEAF Theme Studio DSL. `ThemeFileService.cpp` parses bounded `palette` plus optional `launcher` extension from SD, ignoring large base64 image resources during firmware rendering. The physical LCD cannot use the Studio's decorative *virtual phone housing* assets as a hardware frame.

## 2. Home geometry — absolute physical pixels

Origin top-left `(0,0)`; rectangle `(x,y,w,h)` is right/bottom exclusive.

| Element | x | y | w | h | Rendering note |
|---|---:|---:|---:|---:|---|
| Status bar | 0 | 0 | 240 | 27 | title left; centered unsynced `--:--`; WiFi if present, battery outline only |
| Header seam | 0 | 27 | 240 | 1 | thin light green/selected theme edge |
| Clock card | 12 | 43 | 216 | 78 | #DEF29C on reference Lime, white border |
| Clock inner text | 18 | 50 | 204 | 43 | derived NTP/local time, never guess wall clock |
| Date baseline region | 18 | 91 | 204 | 22 | `Date not set` until NTP |
| Network card | 12 | 132 | 216 | 46 | #B4DE73, clipped WiFi text + count |
| Quick WiFi | 8 | 191 | 72 | 67 | 36×36 icon at tile + (18,6) |
| Quick Music | 84 | 191 | 72 | 67 | same icon space / caption baseline |
| Quick Files | 160 | 191 | 72 | 67 | same icon space / caption baseline |
| Shortcut hint | 8 | 266 | 224 | 20 | clip line to fit text; screenshot-style dark band |
| Three softkeys | 0 | 298 | 240 | 22 | 80px each; `Menu | Open | Quick` |

**Partial updates:** `idleClock()` changes only the clock-card interior; `idleNetworkStatus()` changes the network text + WiFi-badge zone; `idleShortcutDelta(old,new)` repaints **only** the two changed quick shortcut rectangles and **does not** repaint the hint/footer. All Home shortcut icons use the same `drawIcon()` entry point as Menu and app lists.

## 3. Menu geometry — absolute physical pixels

| Element | x | y | w | h | Behavior |
|---|---:|---:|---:|---:|---|
| Menu status | 0 | 0 | 240 | 27 | inherited header typography and WiFi/battery outline |
| Grid viewport | 1 | 28 | 234 | 264 | 3 cols × 4 rows; 76px visible cell + 2px gap |
| Cell `(col,row)` | `1+78×col` | `28+66×row` | 76 | 66 | background band from row; optional selection frame |
| Icon `(col,row)` | `21+78×col` | `31+66×row` | 36 | 36 | same procedural glyph for Home and list |
| Caption `(col,row)` | `3+78×col` | `72+66×row` | 72 | 16 | centered by pixel width, never raw byte count |
| Scrollbar | 236 | 34 | 2 | 252 | built-in 3x4 grid rail |
| Footer | 0 | 298 | 240 | 22 | `Options | Open | Exit` |

Grid order is **WiFi / Bluetooth / Music; File mgr / Gallery / Internet; Shell / Recovery / Settings; Themes / Apps / Library**. MENU toggles between Home and Menu; A/B from Menu returns Home. OPTION opens existing context menu, including optional tabbed explorer. SELECT hold >600ms retains Game/T9 behavior from the base firmware.

**Partial updates:** `LauncherApp::handle()` repaints only `gridItem(old,false)` and `gridItem(next,true)` when changing focus (original placement preserved). A formerly selected cell now restores its original row separator on redraw. For reference Lime the selected tile has a light border and **two short corner glints**, not a full-width opaque streak.

## 4. Exact reference color samples (theme Lime / RGB565)

| Component | RGB565 | Reference RGB888 approx |
|---|---|---|
| Status / dark band | `0x2B63` | `#296D18` |
| Clock/selection | `0xDF93` | `#DEF29C` |
| WiFi status | `0xB6EE` | `#B4DE73` |
| Shortcut background | `0xAEE9` | `#ACDE4A` |
| Menu row 1 | `0x75A3` | `#73B618` |
| Menu row 2 | `0x8E25` | `#8BC629` |
| Menu row 3 | `0x96A6` | `#94D631` |
| Menu row 4 / footer surround | `0x8E44` | `#8BCA20` |

The installed user's `.vqeaf` theme overrides these colors. The built-in Lime palette matches the screenshot on first install; **pre-existing NVS theme choices remain unchanged**. Files `sd/System/Themes/*.vqeaf` are available to copy to SD card.

## 5. Shared font and icon policy

- `UiTypography.h` is the single authority for header, caption, body and hint roles and critical text baselines.
- TFT_eSPI's native ASCII fonts remain the fast path for English and numeric labels, consistent across shared Home/Menu/chrome/list/footer widgets. Mixed Vietnamese UTF-8 NFC names use the same role sizes through `UiVietnameseFont.h` (229 distinct glyphs ×2 generated bitmap weights, no runtime TTF or heap use). DejaVu source attribution is in `third_party/DejaVu_Glyph_Attribution.txt`; **no original font binary is distributed**.
- `SymbianUI::fitTextPixels` clips by drawn width and removes complete UTF-8 codepoints; a malformed or unsupported glyph falls back to `?` instead of splitting a multibyte sequence. Other legacy per-app custom `tft.print` call sites remain candidates for future full translation migration.
- `UiIconCatalog.h` contains canonical IDs/aliases. All 12 Menu icons and 3 Home shortcuts and all shared list icons use the one `drawS60MenuIcon()` 36×36 pixel renderer, independent of the selected theme. Third-party `.qeapp` icons remain their **signed 32×32 RGB565** assets in the app catalog for publisher identity and verification.
- Optional Retro Explorer retains its own small initials/badge treatment intentionally; primary Home/Menu and standard app lists now use the same procedural icon family.

## 6. Verification and comparison limits

```powershell
cd VQEAF-OS
python tools\test_ui_v23.py   # host C++11 full raster, reference-palette compare + all v22 regressions
pio run -e vqeaf_os      # target PlatformIO build; NOT run in this environment
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

- Host C++ framebuffer-test: 240×320 exact area, matching RGB565 anchors, no screen-wide redraw from Home shortcut or Menu focus changes, Vietnamese decoding/clipping, 12 canonical icon IDs.
- Supplied screenshot is cropped at **native 240×320** from the user's image for Home & Menu. The same stable RGB888 palette colors cover **95.94% of Home** and **96.69% of Menu** screenshot-color-mask pixels in the latest host render. This score **does not measure font antialiasing, live hardware electrical behavior, exact scroll FPS or true 1:1 image similarity**. See `preview/v23_side_by_side_reference_vs_host.png` for transparent side-by-side comparison. Small text shapes from the PC stub approximate hardware TFT_eSPI fonts.
- `test_ui_v22.py` remains the base gate and includes `.vqeaf` Studio import, secure `.qeapp` signing/verification scenarios, WiFi failure modes, keypad T9, recovery and 25-source-unit mock link.

### Required device pass before calling the result production-ready

1. Compile actual PlatformIO target (the original board toolchain is not present in this runtime).
2. Upload to the ESP32-S3, verify ST7789 native portrait and `setRotation(0)` physically.
3. Photograph Home and Menu at native crop; confirm font weight/spacing/brightness/glints.
4. Exercise 60 Home shortcut transitions + 240 Menu focus transitions and measure frame pacing/heap.
5. Test no SD, corrupt oversized `.vqeaf`, valid Studio files, invalid/unsigned `.qeapp`, WiFi reconnection, SELECT 600ms & safe recovery chord.
6. Never claim a hardware battery percentage unless an actual ADC wiring exists.
