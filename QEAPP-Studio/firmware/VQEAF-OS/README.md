# VQEAF OS v2.4.2 — QEAPP / browser / theme core recovery

This update fixes the source-level flows used for installing signed `.qeapp` packages,
browsing/downloading small HTML pages, and importing/applying `.vqeaf` themes.
It preserves N16R8 board GPIO, portrait 240×320 UI, RGB565/RLE pixel icons,
existing QEAPP/2 signature checks and theme syntax. This is **not** a Symbian
SIS/SISX or full JavaScript browser runtime. Details: `docs/CORE_FIX_V242_VN.md`.

Run `python3 tools/verify_v242.py` and `python3 tools/doctor_v242.py`.
PlatformIO target compilation, actual TLS, SD, and ESP32-S3 hardware behavior
must still be verified on a physical board; host tests cannot establish these.

# VQEAF OS v2.4.0 — Signed application manager (S60-inspired)

New: verified QEAPP/2 update/rollback recovery, app version comparison,
independent per-app data slots, install progress and app-data reset UI.
Source: `docs/QEAPP_S60_APP_MANAGER_V240.md`. Run `python3 tools/verify_v240.py`.
This does **not** load original Symbian `.sis` packages or native third-party
code; installed QEAPP/2 remains signed `web`/`text` only. Target PlatformIO
ESP32-S3 build and board testing remain pending in this release environment.

## Previous release: v2.3.5 — Lossless Pixel Icon Compression

This release retains **all 12 pixel-art icons** in 24×24 and 36×36 at exactly the same RGB565 pixels and positions as v2.3.4. Icon palettes + compressed sprite streams shrink from **8,260 to 6,443 bytes (−22.00%)** by cropping transparent edges, flattening cross-row RLE and selectively packing four-bit indices. No icon PNGs, focus colors, theme `.vqeaf` or signed `.qeapp` data were modified. Validate with `python tools/verify_icon_optimization_v235.py` and see `docs/ICON_COMPRESSION_V235.md`. Target PlatformIO build still requires the ESP32 toolchain.

## Prior release

# VQEAF OS v2.3.4 — New 12 Pixel Art System Icons

System icons (36px Menu/Home, 24px system lists) now use pixel illustrations
curated from the user-generated icon sheet. Both sizes are checked-in PNGs;
a generated RGB565 per-sprite RLE header is compiled into Flash (~8,260 bytes
of palette+run data). **The original portrait 240x320 Home/Menu layout, .vqeaf
colors, signed .qeapp apps, ESP32-S3 GPIO and v2.3.3 offline/dry-run tooling
remain intact.** This update only changes the icon art and its renderer.

See `docs/PIXEL_ART_ICONS_V234.md` and
`preview/v234_home_menu_contact_sheet.png`. Rebuild/check the sprites with
`python tools/rebuild_vqeaf_icons.py` and `python tools/test_board_host_smoke.py`.
Target PlatformIO compilation and hardware testing remain outstanding.

> **Dry-run update (v2.3.3):** `tools\build_offline.bat --dry-run --host-tests --buildfs` mô phỏng preflight → clean → compile → link → firmware check → buildfs **không cần PlatformIO/toolchain**; chỉ kiểm tra tĩnh GPIO/board là chạy thật bằng Python. Xem `docs/DRY_RUN_PLATFORMIO_V233.md`. Kết quả `DRY_RUN_COMPLETE` không phải build firmware.

> **Offline builder update (v2.3.3):** `tools/build_offline.bat` / `tools/build_offline.py` kiểm tra môi trường cache trước khi build và luôn xuất `build_reports/offline/report.md` + `report.json`. Xem `docs/OFFLINE_PLATFORMIO_BUILD_V233.md`. Bản phát hành không kèm compiler, thư viện bên thứ ba hay `firmware.bin`; cần chuẩn bị cache riêng trên máy cùng hệ điều hành.

> **N16R8 build update (v2.3.3):** see `docs/BOARD_BUILD_V233.md`; custom board `boards/vqeaf_s3_n16r8.json`, partition CSV, and reproducible `tools/build_pio.py`. PlatformIO availability and target build status are recorded separately from host tests.

# VQEAF OS v2.3.2 — Home/Menu icon integration verified on C++ host

**Home:** shared 36x36 WiFi/Music/Files icon assets. **Menu:** all twelve 36x36
system icon assets via one semantic typed slot map, preserving 3x4 at 240x320.
System lists continue to use independent 24x24 assets. Icon RLE and signed
QEAPP/2 .qeapp package formats are unchanged; .vqeaf focus/selection colors
continue to come from the selected theme.

- `src/core/UiIconCatalog.h`: immutable typed Home/Menu asset tables.
- `src/core/SymbianUI.cpp`: direct theme-safe integration, legacy fallback kept.
- `tools/vqeaf_host/test_icon_routes.cpp`: pixel-for-pixel integration checks.
- `tools/test_icon_integration_v232.py`: actual GUI C++ host-render previews,
  asset parity, 36/24 size and v2.3 regression checks.
- `preview/v232_home_240x320.png` and `preview/v232_menu_240x320.png`: exact-size
  RGB565 **PC simulation renders** from production UI C++; not board photos.

Run `python tools/test_icon_integration_v232.py` (needs g++ & Pillow).
Build device locally with `pio run -e vqeaf_os` and monitor at 115200;
PlatformIO and physical device testing are not included in the host gate.

---

# VQEAF OS v2.3.1 — Standardized 12-icon pack (36x36 / 24x24)

**New in v2.3.1:** System Home and Menu use original, pixel-aligned VQEAF icon art. Built-in Menu/Home core glyphs render at 36×36; core glyphs in lists render at 24×24. All 12 are RLE RGB565 compiled into flash (~3,980 asset bytes total), with no dynamic icon buffers. Focus is drawn around the existing menu tile and respects the active `.vqeaf` UI theme; installed signed `.qeapp` packages and their existing 32×32 icon section remain unchanged. Extras still use the v2.3 legacy icon fallback.

* Integration files: `src/core/VqeafIconRenderer.h`, `.cpp`, generated `VqeafIconData.h` and `src/core/SymbianUI.cpp`.
* Original editable/reproducible kit: `tools/icon_assets/` (SVG, PNG, little-endian RAW565, generator and documentation).
* Test: `python tools/test_vqeaf_icons.py` and `python tools/test_ui_v23.py`.
* Rebuild generated art after changing its shapes: `python tools/rebuild_vqeaf_icons.py`.
* Device build: `pio run -e vqeaf_os` (requires local PlatformIO installation).
* Scope: icon standards only; hardware GPIO, panel, radio functions, `.vqeaf` grammar and signed `.qeapp` verification are unchanged.

---

# VQEAF OS v2.3 — Pixel-aligned Home/Menu + shared font/icon system

**Implemented source release, not flashable firmware.** See `docs/UI_HOME_MENU_PIXEL_V23.md` for exact user-screenshot coordinates and `docs/FONT_ICONS_V23.md` for icon/font integration. Run `python tools/test_ui_v23.py` to verify the new C++ host-renderer visual and incremental focus gates, then all v2.2 regressions. Open `preview/v23_side_by_side_reference_vs_host.png` for the supplied screenshots versus **real firmware GUI C++ code rendered by a host TFT stub**. PlatformIO target compilation and real-device tests remain pending. Existing SD `.vqeaf` and trusted QEAPP/2 `.qeapp` behavior is preserved.

## Version notes

Fresh-install Lime default now matches the user reference; any existing selected theme remains unchanged. Every stock skin uses one 36×36 procedural icon family. Header, footer, lists, grid and Home use shared typography definitions and bounded Vietnamese UTF-8 bitmap fallback. Home selection repaints only the two modified tiles; Menu selection retains two-cell redraw with corrected row seam/corner glints. This update does **not** rebuild unrelated app bodies.

---

# VQEAF OS v2.2 — Portrait Home + 3×4 Menu (C++/PlatformIO)

**Firmware source, not a simulated OS rewrite.** Based on your supplied `SymbianS3_OS_v2.0.1_board_test_kit(1).zip` and the later VQEAF OS v2.1 integration; all existing system services, settings storage, verified QEAPP/2 installer and pin assignments are retained. The **normal boot screen is again the reference-image-style Home dashboard**. MENU opens the **3×4 grid** in the same order as the supplied screenshot. The optional 6-tab Retro-inspired **Explorer** remains accessible at `Menu → OPTION → Retro Explorer`, without replacing the reference layout.

**Hardware:** ESP32-S3-WROOM-1 N16R8 · ST7789 240×320 **portrait** (TFT_eSPI, rotation 0) · SDMMC 1-bit · 10 physical buttons · PSRAM 8 MB · Serial 115200. No new GPIO assignments, emulator framework or large GUI dependency.

## Implemented in source

- `src/main.cpp`: Home-first boot, physical MENU → 3×4 Menu (MENU within Menu → Home), existing physical recovery chord, separate optional `ScreenId::Explorer` routing; current hardware/service initialization stays intact.
- `src/apps/LauncherGrid.cpp`: real 3×4 Menu logic from your earlier v2.0.1 firmware, focus updates repaint **only old/new cells**. The optional VQEAF tab Explorer's original implementation remains in `src/apps/Apps.cpp`/`src/launcher/LauncherView.*`.
- `src/core/UiLayoutGeometry.h`: **central pixel contract** for 240×320, header/footer, 3×4 grid, 6×42 list rows, Home cards and major app viewports. `src/core/SymbianUI.h` now aliases its shared dimensions to this file and exports `using VqeafUI = SymbianUI;` for compatibility.
- `src/core/UiScreenAtlas.h/.cpp`: **16 screen/162 target region** programmatically accessible catalog, generated together with `docs/pixel_atlas.json` and `docs/UI_PIXEL_ATLAS_V22.md`. Regions not yet explicitly used by legacy individual app draw methods are **target geometry**, not a claim of a complete 16-screen pixel-perfect reflow.
- `.vqeaf` RGB565 palette + `launcher{}` fields are now mapped to **physical LCD Home/Menu/app chrome, content background, selected text, panel, border and softkey bar** (as well as optional Explorer). Theme Studio's 3D phone shell/keypad imagery remains **virtual preview artwork** and is not overlaid on the real LCD. Sample `vqeaf_reference_lime.vqeaf` is included on SD.
- Existing QEAPP/2 binary signed app service, secure launch gate, URL browser, WiFi, BLE, Files, media, Shell, Recovery and original settings persist unchanged. An arbitrary Lua/ZIP `.qeapp` **is not supported** by this firmware.
- Battery status is only an **outline**: no battery ADC reading is documented for this hardware, so the UI must not invent a charge percentage.

## Primary UI / keys

```text
Power-on -> Splash -> [Recovery when needed] / Home General
HOME: LEFT/RIGHT WiFi, Music, Files shortcut; START Open; MENU 3x4 Menu
MENU: 3 columns x 4 rows; D-pad focus; START Open; A/B/MENU Home
MENU -> OPTION -> Retro Explorer (optional six-tab navigator)
APP: MENU returns 3x4 Menu; OPTION context; A Back
SELECT hold (>600ms per board policy) toggles Game/T9
```

**Main grid order**: WiFi / Bluetooth / Music; File mgr / Gallery / Internet; Shell / Recovery / Settings; Themes / Apps / Library.

## Build and flash (PlatformIO on developer's PC)

```powershell
cd VQEAF-OS
python tools\test_ui_v22.py       # Host test on Python + g++ (PC / Linux; Windows requires g++)
pio run -e vqeaf_os                # Required real firmware build (requires PlatformIO)
pio run -e vqeaf_os -t upload      # Requires connected board
pio device monitor -b 115200
```

The unchanged `platformio.ini` uses `TFT_eSPI` and the existing NimBLE, TJpg, PNGdec dependencies. No flashable `.bin` is included because the target toolchain and physical board were **not available** here. Python host tests are **not equivalent** to PlatformIO target compilation or real-screen testing.

Themes: copy `sd/System/Themes/*.vqeaf` to `SD:/System/Themes/`, then open `Menu → Themes`. To install `.qeapp`, read `docs/QEAPP_V15_SIGNING.md` first; only signed/approved packages can be installed. Default OS themes and samples are included for offline checks.

## Pixel spec and previews

- `docs/UI_PIXEL_ATLAS_V22.md`: **individual X/Y/W/H/token table for all 16 screens**.
- `docs/pixel_atlas.json`: machine-readable target data.
- `tools/gen_pixel_atlas.py`: regenerates data, Markdown and C++ atlas from one definition.
- `preview/v22_16_screen_pixel_blueprints.png` and per-screen `preview/v22_*.png`: **design schematics**, not simulator output and not hardware screenshots.
- `docs/UI_RELEASE_V22.md` & `../VERIFY_HOST_V22.log` when provided: change details, verified/pending checklist.

## Incomplete / next pass

Some inner apps retain their previous per-page coordinates and older built-in artwork; the pixel atlas provides the next migration contract. The TFT_eSPI built-in text fonts do not ensure complete Vietnamese UTF-8 coverage. The optional Explorer uses direct redraw and its own geometry. Hardware FPS/SD hot-plug/radio/serial safety and PlatformIO target compatibility must still be measured before distributing a ready-to-flash binary. Source names such as `SymbianUI` and existing Preferences namespaces are intentionally **internal compatibility aliases**, not user-facing branding. No Retro-Go source code or visual assets have been copied.

## v2.3.6 — on-device icon verification and whole-firmware size

See [`docs/ICON_DEVICE_AND_FLASH_MEASUREMENT_V236.md`](docs/ICON_DEVICE_AND_FLASH_MEASUREMENT_V236.md).

```powershell
py -3 tools\verify_icon_device_v236.py
pio run -e vqeaf_icon_selftest
pio run -e vqeaf_icon_selftest -t upload
py -3 tools\capture_icon_selftest.py --port COM5
py -3 tools\measure_firmware_icon_impact.py
```

The last command **builds two full firmware variants**, recording actual
`firmware.bin` deltas. Without PlatformIO it reports UNAVAILABLE. The `vqeaf_os`
production environment remains optimized and contains no self-test payload.


## Pixel Snake signed QEAPP template (opt-in firmware extension)

A bounded native Pixel Snake game handler is included in v2.4.1. The demo
`games/pixel_snake/dist/snake_pixel_demo.qeapp` requires the **separate**
`vqeaf_snake_demo` firmware environment. Existing `vqeaf_os` publisher pin is
untouched. The signed package contains only the game configuration and icon;
**no general runtime for game scripts is implied.** See
`games/pixel_snake/README_VN.md`.
