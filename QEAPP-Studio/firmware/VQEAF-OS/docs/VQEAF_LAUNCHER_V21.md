# VQEAF OS v2.1 — 240×320 Retro-Go-inspired portrait launcher

## Target and licensing

This package updates the **actual user-supplied SymbianS3_OS v2.0.1 board-test-kit firmware**, keeping the existing Arduino/TFT_eSPI/PlatformIO toolchain, ESP32-S3 N16R8 GPIOs, board diagnostics, functional built-in apps and signed .qeapp installer. It creates an **independent original launcher inspired by the architecture of Retro-Go**: tab categories, vertical list, selected-row highlight, selection preview and system dialogs. We do not bundle Retro-Go source, default theme, fonts or images. If copying Retro-Go GPL-2.0 code in a future version, the applicable open-source licensing obligations must be handled separately.

`include/BoardConfig.h` preserves every documented pin and `TFT_ROTATION=0`; do not replace it with Retro-Go's horizontal 320×240 layout. Project-local PlatformIO `env:vqeaf_os` is the build target. Legacy class names `SymbianUI` and NVS key prefixes `symbian-*` remain **internal compatibility details**, NOT public OS branding, so existing installed user settings and factory data are not wiped.

## On-device 240×320 geometry

| Viewport | Y px | Layout |
|---|---:|---|
| Status | 0–23 | VQEAF OS, NTP time, WiFi, SD and battery outline |
| Tab logo + title | 24–81 | 42×42 procedural emblem, active category, six page dots |
| List | 87–231 | 5×29px rows, 3px visible scrollbar, color highlight |
| Preview | 237–295 | Cover-style 39px badge or installed signed app's actual RGB565 icon, summary |
| Actions | 297–319 | OPTION / OK Open / A Back |

Display is immediate-mode. `LauncherView::full` paints the full tab only at entry/tab switch/offset changes. `LauncherView::row` repaints two rows on focus changes; `preview` updates only its pane. A popup paints a bounded dialog. The firmware **does not implement Retro-Go's framebuffer DMA or its RGB565 theme.json runtime**; it uses the supplied TFT_eSPI driver as-is, intentionally limiting migration risk. Animated frame overlays and full-screen theme background decoding are not yet ported.

Tabs: Home | Internet | Applications | Media | System | Settings. Apps/Utilities from v2.0.1 remain usable. The Applications tab additionally appends verified installed QEAPP/2 packages. File manager, gallery, WiFi wizard, music, BLE, browser, calculator, stopwatch, recovery and shell retain their existing underlying screens and persistence. Their existing chrome receives the selected palette, but **not every inner application view has yet been converted to the new launcher geometry**.

Keypad (unchanged hardware): MENU=return Home launcher; UP/DOWN=move; LEFT/RIGHT=switch tabs; START=launch; OPTION=context menu; A=close/go Home tab; B=item details. In a text editor, SELECT hold >650ms toggles Game/T9. Numeric keys use existing multi-tap when T9 is enabled. DOWN or A held at cold boot forces recovery/Safe Mode (no pins changed). On other legacy applications the B button may retain the old secondary/Back behavior.

## Standard `.vqeaf` imports

The source format is **VQEAF Theme Format 1.0**, `@vqeaf 1.0` followed by `<theme id="..." name="...">`. This is **not XML** and **not Retro-Go `theme.json`**. Existing VQEAF Theme Studio exports have a `palette { ... }` (typically at the beginning of the file) containing these keys:

| Studio token | VQEAF OS display meaning |
|---|---|
| `screen` | Main content + launcher background |
| `shellTop` | Status/header |
| `shellBottom` | Popup/preview background |
| `shellBorder` / `keyBorder` | Borders |
| `key` | Panels |
| `keyPressed` | Selected list |
| `keyText` | Main/header text |
| `subText` | Secondary text |
| `accent` | Active tab/scrollbar |
| `glow` | Flat fallback warning accent; no true glow rendered |

The loader accepts valid 1.0 `#RGB`, `#ARGB`, `#RRGGBB` and `#AARRGGBB`, converts RGB to RGB565 and ignores alpha in flat-color mode. Unknown palette fields are ignored. For the optional **firmware-only** appearance extension, place this block after `palette` and before any very large `<resource>` sections (Theme Studio may omit this block on re-export):

```vqeaf
launcher {
  background: "#080F1A"
  foreground: "#F0F7FF"
  headerBg: "#102540"
  headerFg: "#F0F7FF"
  tabAccent: "#55D2C6"
  listBg: "#080F1A"
  listFg: "#D5E6F7"
  selectedBg: "#235481"
  selectedFg: "#FFFFFF"
  previewBg: "#152C44"
  previewFg: "#FFFFFF"
  scrollbar: "#55D2C6"
  footerBg: "#102540"
  footerFg: "#FFFFFF"
  border: "#355277"
}
```

The text parser does **not execute** input. It validates magic/closing tag, scans at most the first 16 KiB of the textual header, and limits the total theme file to 512 KiB so Theme Studio base64-embedded resources can exist without occupying the heap. Embedded data-URI PNG/WebP, vectors, decorations, shell/keypad visualization, float/blur/glow and animated effects are *ignored*, not displayed on the device. Resource data is still preserved verbatim in the source `.vqeaf` file. A missing SD, broken file or unsupported palette does not modify an already valid running theme; an invalid saved external theme falls back to VQEAF Night visually.

Copy files to microSD `/System/Themes/*.vqeaf` (preferred) or `/Themes/*.vqeaf` (compatibility). Sample `sd/Themes/vqeaf_night.vqeaf` and `vqeaf_day.vqeaf` are supplied. To use full VQEAF Theme Studio assets you would need a future embedded raster-resource decoder, separate from this release.

## `.qeapp` compatibility

The provided v2.0.1 firmware already implements **QEAPP/2**, a signed binary container. Its manifest is text `key=value` inside the signed binary; it is *not* a ZIP archive or arbitrary `main.lua` file. This release deliberately leaves the on-disk format unchanged. Apps are declarative HTTPS web launchers or bundled text documents; all catalog validation, pinned publisher key, ECDSA signature checks and use-time verification remain in place. A valid installed app with a 32×32 RGB565 icon shows its real signed icon in the launcher preview, cached as 2 KiB for the selected item only. Build/sign your own apps following `docs/QEAPP_V15_SIGNING.md`; never ship a private signing key.

## Future extensions (not shipped)

- Apply new tab/list/preview geometry to inner app screens, including browser.
- Decode a bounded `.vqeaf` wallpaper/image resource to the physical LCD.
- Dedicated bookmarks/history/favorites in launcher (browser keeps its existing UI).
- True dirty-rect framebuffer DMA and Vietnamese glyph font metrics; these are not implied by a simple UI retheme.
