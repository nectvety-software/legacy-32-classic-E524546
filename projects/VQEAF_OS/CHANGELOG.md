## [2.5.0] — Feature-phone UI / graphics response (host candidate)

- Opaque 24/36px icon scanline streaming (72-byte stack row, one batched TFT transaction/icon); original transparent renderer and A/B compatibility profile retained.
- RGB565 byte-order restoration; Home/Menu/list switched only on flat-background system icon paths.
- Non-blocking wall-clock formatting and shorter Opening interstitial (170ms -> 32ms).
- Cooperative yields during long JPEG/PNG/BMP decode callbacks.
- Opt-in `vqeaf_perf_diag` with real-board loop-latency and heap telemetry; mock test cannot establish FPS.
- No hardware GPIO, signature key, QEAPP/2 package, or VQEAF theme contract changed.
- Tests and board protocol: `docs/FEATURE_PHONE_RENDER_CORE_V250_VN.md`.

## v2.4.2 — Verified QEAPP, network downloads and theme recovery (2026-09-24)

- Discover signed `.qeapp` files in inbox, Downloads and the microSD root, with
  explicit layout and signing-key mismatch diagnostics; never bypass signatures.
- Preserve clicked browser URLs across link-pool reset and retain retry/history
  for failed requests. Support bounded chunked transfer for pages and downloads.
- Validate `.vqeaf` palette/envelope during catalog scan; leave the currently
  applied palette in place when a candidate fails and route theme downloads to
  manual Apply.
- Expose `corediag` and boot-time checks for SD/WiFi/TLS/signing key.
- Add host verification/doctor tooling. Host gate: 17/17 passing; v2.4.0 gate:
  7/7 passing. Hardware test and real PlatformIO build still pending.

## v2.4.0 — S60-inspired package manager core
- Signed QEAPP/2 higher-version update with staged verification and last-version backup.
- Recovery of interrupted installs/updates; never promote unconfirmed staging.
- Per-app data foundation with fixed slots and strict quotas; explicit user reset.
- App Installer progress on 240x320 and Update/Recover/Reset data controls.
- Fresh host regression for SD rename failure, blocked rollback and data integrity.
- Preserve N16R8 GPIO, `.vqeaf` themes, `.qeapp` format and original icon codec.
- Target PlatformIO compilation and physical board tests have not yet run.

# v2.3.5 — Lossless Flash optimization (system icons)
- 24 assets (12 identifiers × 24/36px): crop transparent margins; flattened RLE with long transparent-run escape; choose 4-bit indices when smaller.
- Sprite+RGB565 palette storage: 8,260 → 6,443 bytes (−22.00%); no change to 16-byte ESP32 asset descriptors or no-heap renderer contract.
- Checked independent old/new C++ framebuffer output and PNG-derived RGB565 output.
- Firmware target and physical device build remain unverified until performed on board/toolchain.

## v2.3.4 — Pixel art illustration icon set (2026-09-24)

- Curated the user-generated contact sheet into 12 transparent 36x36 and
  12 separate 24x24 pixel PNGs; no runtime asset decoder or scaling.
- Replaced legacy token palette RLE with per-icon RGB565 palette + safe,
  bounded 5-bit-index/3-bit horizontal RLE in Flash (8,260 data bytes).
- Existing Home, 3x4 Menu and system-list call sites automatically use the
  new art; .vqeaf handles focus/background and .qeapp signed format stays
  unchanged.
- Updated host screenshot comparison to ignore only the intentionally
  replaced icon rectangles; all new icon pixels have independent equality
  tests against source PNGs.
- Kept v2.3.3 offline and dry-run builders. PlatformIO and board unverified.

## v2.3.3 — N16R8 PlatformIO configuration and reproducible build gates

- Add project-local N16R8 board JSON and explicit Arduino-ESP32 2.0.17 16MB OTA partitions
- Pin PlatformIO 6.10.0 and library versions, set max firmware to real app partition (6.25MiB)
- Add C++11 compile-time board/pin guards and boot-time flash/PSRAM serial diagnostics
- Add offline preflight, compile guard test, 26-unit native host link and exact GUI/icon smoke
- Add optional PlatformIO CI and local one-command build logs with honest PASS/FAIL/BLOCKED states
- Preserve UI assets, physical GPIO, `.vqeaf` and signed QEAPP/2 verifier; see docs/BUILD_REPORT_V233.md

# VQEAF OS v2.3.2 — Typed Home/Menu icon integration (2026-09-24)

- Main Menu now maps all 12 slots directly to the standardized 36×36 RGB565 icon IDs in `UiIconCatalog.h`.
- Home WiFi, Music and Files shortcuts directly share the same 36×36 bitmap identities; system lists retain 24×24 assets.
- Added full-UI host pixel-for-pixel parity tests for every icon location, selected focus changes and `.vqeaf`-compatible custom focus colors.
- Added four native 240×320 C++ host renders. No GPIO, signed `.qeapp`, theme format or app service changes.
- Production PlatformIO toolchain and physical ESP32-S3 LCD tests remain pending.

---

# VQEAF OS v2.3.0 — Reference Home/Menu pixel pass (2026-09-24)

- Default first-install appearance is reference Lime from screenshot; old Preferences keys and selected .vqeaf files remain intact.
- 240×320 Home clock, network, 3 tiles, quick-hint, 3 softkeys use the existing pixel atlas; highlight movement now only repaints two affected Home shortcut rectangles.
- Menu remains 3×4 screenshot order: old/new focus cells only; restore row separator and draw two short selected top-corner glints.
- Unify all stock Home/Menu/shared-list icon rendering through the original 36×36 procedural icon art, plus canonical aliases and missing Doc/Bell/Lock/Quick entries.
- Single text role definitions and bounded UTF-8 NFC Vietnamese glyph raster fallback; UTF-8 safe pixel-width clipping in shared screen and optional Explorer.
- Actual source-host C++ RGB565 framebuffer renders compared with user screenshot native crops; palette color masks match 95.94% Home and 96.69% Menu. Host font differs from live hardware fonts; these figures are not full-image pixel similarity and are not an on-device pass.
- Keep VQEAF Theme Studio 1.0 `.vqeaf` parsing and signed QEAPP/2 `.qeapp` installation + existing WiFi/BLE/Browser/Audio/Recovery and board pin map unchanged.
- Host test `python tools/test_ui_v23.py` PASS (includes previous v2.2 host regressions). PlatformIO/physical ESP32 target: **NOT VERIFIED**.

---

# VQEAF OS Changelog

## v2.2.0 — Screenshot layout & pixel atlas (source release)

- Normal boot: Home dashboard (General) restored. MENU opens physical 3x4 Menu with 12 icons exactly ordered as in user screenshot.
- Retained v2.1 six-tab Retro-inspired view as optional Retro Explorer from Menu > Options, not boot Home replacement.
- Shared C++11 geometry header + 16-screen / 162-target-rectangle generated UI atlas, including a detailed per-screen Markdown and JSON spec.
- Studio `.vqeaf launcher{}` RGB565 overrides reach physical Home/Menu/app chrome and softkeys; signed `.qeapp` service and hardware pins untouched.
- Fixed simulated battery fill: outline-only when no ADC available.
- PASS on host: 25 actual firmware translation units compile/link with mocks, UI atlas, theme parser, T9/input, signed QEAPP tamper, WiFi and board diagnostic. Target PlatformIO + hardware are pending.

## VQEAF OS 2.1.0 — New portrait launcher (source-only, host verified)

- Rebrand splash/About, boot into original six-tab Retro-Go-inspired launcher, with 240x320 linear list, preview and modal, preserve original firmware services.
- Default VQEAF Night; Studio .vqeaf palette and optional launcher token overrides with bounded streaming large-file parser.
- Signed existing QEAPP/2 apps integrated in launcher with existing verifier and payload restrictions unchanged.
- SELECT hold mode toggle and deferred short press with editor-only physical numeric multi-tap.
- Added new host verification and device test guide; PlatformIO and physical board remain unverified.

---

## v2.1.0 — VQEAF OS portrait launcher and .vqeaf integration

- New VQEAF OS brand in user-visible boot, About, BLE, browser UA and Shell.
- Original six-tab 240x320 portrait launcher with list highlight, scrollbar,
  selected application preview, full-screen status, popup and signed QEAPP/2 icon.
- Theme Studio `.vqeaf` palette import with optional firmware-only launcher
  extension and fallback to an embedded dark theme.
- Existing signed QEAPP/2 web/text verifier and all earlier core services retained.
- Retain NVS storage namespace across upgrades; DOWN/A hardware Safe Mode chord.
- No physical build/board claims: see verification report.

## v2.0.1 – On-board verification kit

- Added production-CA TLS diagnostic probes with DNS/NTP gates and certificate-specific negative verdict requirements.
- Added safe 4096-byte SD scratch readback/CRC32 test with collision avoidance and Serial remove/remount event logs.
- Added bounded serial `diag` console, on-screen Shell commands, Windows pyserial live test collector and host regression.
- **No hardware test results claimed:** real board, internet and card hotplug need user-side runs.

## v2.0.0 — SD recovery and verified HTTPS

- Replaced insecure Browser and Shell HTTPS clients with verified mbedTLS CA
  chains, seven audited embedded Mozilla root certificates and an NTP time gate.
- Blocked HTTPS downgrade and plaintext downloads; `.qeapp`/theme downloads
  now require `Content-Length`, bounded stream and staging readback. Old
  insecure browser cache entries are not reused.
- Added bounded microSD health polling, rate-limited no-card boot retry and
  detection notifications. Never deinitialize/remount while WAV playback or
  pause holds a File. Re-mounted cards restore standard folders and app list.
- Atomic file updates re-read `.tmp` before replacing the previous `.bak`
  protected version. Shell exposes `sd` mount/error diagnostics.
- Added executable host regression for card removal/insertion, staged write
  corruption, CA fingerprint verification and untrusted certificate rejection.
- 21/21 C++ production units link with mock host APIs, NOT target PlatformIO.

## v1.9.0 — OS stability, cache crash recovery, networking hardening

- Fixed Qeafbrowser indefinitely waiting for the first page body byte after
  an HTTP 200: 8s inactivity and 25s stream budget, missing-body/error status
  checks, 32KB declared-length validation and truncated response rejection.
- Collect Content-Type headers on the ESP32 HTTPClient path, and do not mask
  actual HTTP 4xx/5xx replies with cached pages. Removed a duplicate malloc.
- Reject silently truncated/unsafe URLs; correct domain-only query URLs,
  query-only links, and relative dot-segments. Reject unsupported URI schemes.
- Stop download completion overwriting an existing theme or app package.
- SD atomic replacements now use .tmp + .bak and read-time recovery/rollback.
  The cache pruner counts streamed entries instead of first N listing results;
  Shell cache clear is streaming up to an explicit 128-file safety cap and
  displays errors when it cannot finish.
- WiFi NVS filters invalid/duplicate profiles at boot, repairs corrupt counts
  once, and avoids five-profile rewrites when a connection is unchanged.
- Nokia-inspired heavy glyph overlay draws subsequent passes transparently;
  formerly the solid background could erase first-pass strokes.
- Unsynced clock shows --:-- / Date not set instead of invented uptime HH:MM;
  display updates with a notification after NTP really synchronizes.
- Native C++ tests for URL, HTTP timeout/status/truncation, POSIX SD recovery,
  large cache cleanup and WiFi NVS wear/corruption; full host syntax/link gate.
  **No claim of target PlatformIO success or actual LCD/audio/RF validation.**

## v1.8.0 — Utilities / Multimedia / BLE refinement
- Added CalculatorEngine (bounded numerical input, safe math and on-screen 4x4 keypad).
- Added wrap-aware StopwatchEngine, 4 laps, 100ms dirty time redraw, task resume.
- Music: Now Playing, progress, volume, bounded no-repeat shuffle/repeat and background playlist advance.
- Music: finish delay for I2S DMA, natural-end event; no new screen framebuffer.
- Gallery: timed 4-second slideshow and external File Manager path support.
- BLE: strongest 16 scan results, properly closable detail view.
- New portable native tests plus full GNU++11 host link; PlatformIO/device validation pending.

## v1.7.0 – Signal-aware Saved WiFi Auto Connect

- Added nonblocking boot auto-scan and strongest-saved-AP selection across the complete scan list, with last-used SSID as a tie-break only.
- Tries lower-RSSI saved APs on rejection/timeout; no unverified passwords are persisted.
- Re-scans after a 5-second link-loss debounce; exponential retry wait is capped at 120 seconds (30/60/120s).
- Honors Settings Auto WiFi, Safe Mode and intentional Disconnect/radio-off; browsing the WiFi list alone is reversible.
- Added S60 240x320 standby WiFi status text: scanning/joining/SSID+RSSI/retry countdown. Dirty-region repaint for WiFi line and right icons preserves anti-flicker behavior; foreground titlebars also refresh the WiFi badge without repainting the title or clock.
- Direct Shell WiFi radio commands pause automatic selection, preventing competing scans/joins; simply browsing the WiFi list resumes after leaving it.
- Added `tools/wifi_host/test_auto_wifi.cpp`, `tools/test_v17_auto_wifi.py` and `docs/WIFI_V17_AUTOCONNECT.md`.
- Existing v1.6 keypad connection wizard, Themes, Browser, Shell and signed QEAPP installer retained.

# Changelog

## v1.6.0 — Qeafbrowser-style WiFi wizard

- New `WiFiConnectionService`: non-blocking scan/connect, retains active association during scan, top-16 RSSI across full scan including hidden SSIDs, 12s timeout.
- WiFi UI: saved network list, hidden SSID entry, progress/result views, network status, Rescan/Forget/Disconnect menu and partial redraw when scrolling.
- Five existing NVS profiles persisted after successful verified join only; same-SSID replacement password must reconnect to validate.
- WiFi auto-reconnect preference and safe-mode restrictions; no password logging.
- 12 runtime WiFi host tests + 16 static integration tests and full GNU++11 host link. Not yet verified on real ESP32-S3 hardware.


## v1.5.0 — QEAPP/2 publisher signatures

- Firmware-pinned ECDSA P-256 public key, verified by ESP32 mbedTLS.
- Signed 76-byte QEAPP/2 trailer, covering complete package header, manifest, icon and payload.
- Block old unsigned QEAPP/1, unknown key IDs, invalid/forged signatures, broken hashes and truncated packages.
- Signed install receipts and complete hash/signature revalidation of installed content at boot and at launch.
- Package rejected UI with actionable error, signature-verified package details, two signed sample packages.
- Local key provisioning and package-signing CLI. No private key in release.
- GNU++11 host OpenSSL integration and negative tamper/legacy/foreign-signer tests.
- Toolchain mock checks are not an ESP32 target build or on-board hardware validation.

## v1.4.0 — QEAPP App Installer

- New versioned QEAPP/1 binary package with strict bounded manifest,
  optional 32×32 RGB565 icon, and optional bundled text.
- Streaming SHA-256 verification before install and during SD copying;
  staging-directory commit, collision-safe IDs, no implicit overwrite.
- App Installer Inbox/Installed UI with verified details, preview,
  confirmation, removable entries and fixed 12-app catalog.
- File Manager and Qeafbrowser download handoff; Applications launcher
  integrates installed web/text apps and uses existing opening animation.
- `docs/QEAPP_V14.md`, sample packages, deterministic builder and
  host tests. **No native execution; SHA256 not a signature.**

Earlier versions: v1.3 SD Platform, v1.2 Themes, v1.1 AMOLED Red, v1.0 S60 Green.


---

## v1.3.0 — Bold UI / SD platform / browser cache + downloads

- Added Nokia 2700/S40-inspired bold raster UI without bundling external font files or allocating a font framebuffer.
- Added automatic microSD system directory bootstrap under `/System`, `/Media` and `/Documents`.
- Preferred VQEAF location moved to `/System/Themes` with `/Themes` compatibility retained.
- Added bounded Qeafbrowser HTML cache (16 files / 512 KiB), offline cache fallback and address-bar cache marker.
- Added streaming browser download action with routing to Themes, App Inbox or Downloads and a 4 MiB safety cap.
- Added Applications > App inbox and File Manager shortcuts for Downloads, Themes and App inbox.
- Media apps now prefer standard media/document folders before compatibility root scans.
- Added Shell `layout` and `cache` maintenance commands; `df` now reports free card space.

## v1.2.0 — microSD Themes application

- Dedicated Themes app on S60 3×4 launcher, Applications, Settings and File Manager `.vqeaf` open action.
- Prioritized `/Themes` scan and bounded search through microSD; built-in themes work without card.
- Reusable staged VQEAF palette parser: 16 max theme entries, 32 KiB max file, 400 max lines, input validation and RGB565 conversion. Unknown future palette fields are ignored; recognized colors still require valid #RRGGBB.
- D-pad list + partial selection redraw, Options / Apply / Rescan / Details with correct S60 softkey controls.
- Custom theme selection stored in NVS and reloaded once the SD card mounts on boot; missing/invalid files safely fall back to S60 Green.
- Colorful procedural palette icon for Themes; no large theme bitmap or full-screen framebuffer.
- Added host C++ runtime parser tests (real VQEAF fixture, invalid, oversized, traversal, BOM, palette brace on next line), regression script and microSD examples.
- Kept 240×320 portrait and compact WiFi + battery status indicators.

## v1.1.0 - VQEAF AMOLED Red / compact status icons

- Added AMOLED Red theme using the palette from VQEAF Theme Studio `amoled_red.vqeaf`.
- Grouped WiFi + battery status glyphs at the far-right with a 6 px gap; centered clock remains unchanged.
- Extended S60 3x4 large-icon rendering to AMOLED Red and added allocation-free dark/red menu + standby backgrounds.
- Theme selector now cycles S60 Green, AMOLED Red, Black and Classic beige.
- Added device-adapted `.vqeaf` theme files under `themes/`.

# Changelog

## v1.6.0 — Qeafbrowser-style WiFi wizard

- New `WiFiConnectionService`: non-blocking scan/connect, retains active association during scan, top-16 RSSI across full scan including hidden SSIDs, 12s timeout.
- WiFi UI: saved network list, hidden SSID entry, progress/result views, network status, Rescan/Forget/Disconnect menu and partial redraw when scrolling.
- Five existing NVS profiles persisted after successful verified join only; same-SSID replacement password must reconnect to validate.
- WiFi auto-reconnect preference and safe-mode restrictions; no password logging.
- 12 runtime WiFi host tests + 16 static integration tests and full GNU++11 host link. Not yet verified on real ESP32-S3 hardware.


## v1.0.1 - Centered clock / balanced S60 statusbar
- Reworked the 240px titlebar into three equal 80px zones for title, clock and status.
- Clock is now mathematically centered on the complete 240px display and vertically aligned inside the 27px titlebar.
- WiFi and battery use equal 40px slots; no SIM/cellular indicator is introduced.
- App titles are clipped by rendered pixel width so they cannot overlap the clock.
- Active Standby/Home uses the same top-bar geometry.
- RSSI-only changes repaint the right 80px status zone instead of the whole screen.

## v1.0.0 - S60 Green Theme / portrait 3x4 menu
- Added S60 Green as the new Nokia/Symbian-inspired default theme with a one-time NVS appearance migration.
- Kept the board hard-locked to ST7789 portrait 240x320 and retained WiFi+battery-only status indicators.
- Reworked launcher geometry to 3 columns x 4 rows with 12 direct application entries.
- Increased procedural icon box to 36x36 and added S60-style green menu icons/highlights without bitmap RAM.
- Added deterministic four-band green menu wallpaper so partial focus redraw remains flicker-free.
- Reworked S60 Green titlebar/navbar proportions to 27 px / 22 px and made softkeys pale-green with dark labels.
- Settings now cycles S60 Green, Classic beige and Black; Reset appearance selects S60 Green.
- Retained v0.9 advanced file management, network tools and system/network monitoring.

## v0.9.0 - Shell File + Network Tools
- Added bounded file-management commands: mkdir/rm/rmdir/touch/cp/mv/write/append/hexdump.
- Added quoted shell argument parsing for paths, SSIDs and text containing spaces.
- Added nslookup, ICMP ping with compile-time TCP fallback, streaming wget, netmon and firmware top snapshots.
- Expanded ifconfig with MAC/subnet/gateway/DNS/RSSI and added cumulative shell transfer counters.
- Added direct S60 Shell popup entries for Network monitor, System monitor and File commands.
- Kept copy buffers, hexdump output, terminal output and history strictly bounded for predictable ESP32-S3 RAM use.


## v0.8.0 - Shell Edition
- Added bounded BusyBox-style diagnostic Shell app for ESP32-S3 firmware.
- Added microSD commands (`pwd`, `cd`, `ls`, `cat`, `stat`, `df`) and system commands (`free`, `uptime`, `date`, `uname`, `ps`).
- Added WiFi command control, Safe Mode control, reboot request, command history and S60 app lifecycle integration.
- Added terminal pixel icon and shared keyboard `/` + `:` characters.
- Shell stays available during Safe Mode for repair/diagnostics.
- Architecture note documents why the ESP32-S31 Linux/OpenSBI image cannot be directly reused on ESP32-S3 Xtensa hardware.


## v0.7.0 - Recovery / Library / Qeafbrowser

- Removed BLE/microSD/cellular-style indicators from the S60 status cluster; only WiFi strength and battery remain.
- Added persistent crash-loop tracking using ESP32 reset reason + an NVS `bootPending` marker.
- Added automatic Safe Mode after two repeated panic/watchdog/brownout boots, plus hold-A boot recovery chord.
- Safe Mode skips audio, BLE, Qeafbrowser and WiFi auto-connect while preserving repair-oriented local apps.
- Added Recovery app for reset diagnostics, Safe Mode control, crash-flag clearing and restart.
- Added Gallery with streamed BMP rendering plus PSRAM-backed TJpg_Decoder/PNGdec paths.
- Added bounded, paged Text Viewer and File Manager routing for image/text file types.
- Added Library category navigation for Images / Music / Documents.
- Integrated a Qeafbrowser-derived in-OS browser core with HTTP/HTTPS, explicit 6-hop redirects, fixed line/link pools, 32 KB PSRAM response buffer, keypad URL entry, link focus and back history.
- Preserved case-sensitive `href` values while parsing HTML and added `<img alt>` text fallback.
- Added S60 "Dang mo ung dung" / "Dang tiep tuc" application-opening screen with task-switcher resume semantics.
- Added `tools/test_v07.py`; v0.7 feature/static gates pass 80/80, all 13 C++ translation units pass GNU++11 host syntax checking, and a full host link passes with the PlatformIO TFT macros enabled.
- Retained v0.6.1 anti-flicker rendering, v0.5.1 `ctx` linker fix and v0.4.1 TFT macro fix.

## v0.6.1 - S60 UI / Anti-flicker & Memory pass

- Fixed visible LCD flashing caused by clearing/repainting complete screens during normal key navigation.
- Added partial D-pad redraw paths for Launcher, Applications, WiFi, BLE, File Manager, Collection, Music, Settings, Quick Panel, Notifications and Recent Apps.
- Cached S60 title/status chrome and softkey navbar; unchanged bars no longer redraw on every event.
- Reworked portrait UI metrics: 30 px titlebar, 24 px navbar, 42 px rows and 28 px procedural pixel icons.
- Idle/lock/clock now update only their clock/focus regions and use a 10-second minute-display refresh cadence.
- Replaced the old full-black transition with a small non-destructive accent sweep.
- Keyboard no longer clears its content region on every key press.
- Reduced memory pressure: WiFi/BLE result caps 16 with fixed char storage, File Manager 40 entries, Music 32 tracks, Notifications 8 entries, no Collection `FsEntry[64]` temporary.
- Reduced I2S DMA/work buffers and release NimBLE allocations after scan-only operations.
- Removed TFT_eSPI Font4 from the build; retained Font2 + GLCD/Font1.
- Retained v0.5.1 `ctx` linker collision fix and v0.4.1 TFT macro collision fix.

## v0.5.1 - Linker symbol hotfix

- Fixed `multiple definition of ctx` at link time against ESP32-S3 `libnet80211.a(wl_offchan.o)`.
- Renamed global application context from `ctx` to `appCtx`.
- Made `main.cpp` runtime globals file-local (`static`) to prevent future symbol collisions with ESP-IDF/Arduino libraries.
- No hardware pin, storage format, settings format, or application behavior changes.

## v0.4.1

- Fixed PlatformIO compile failure caused by `TFT_DC`, `TFT_CS`, and `TFT_RST` macro collisions with `BoardConfig.h`.
- Renamed board-side TFT constants to `TFT_*_PIN` without changing GPIO values.
- Replaced C++ aggregate assignment/return patterns with explicit constructors for GNU++11 compatibility (`BtnState`, `KeyEvent`, `FsEntry`).
- Re-ran portrait/grid/keyboard feature gates and C++11 host syntax checks with the same TFT build macros used by PlatformIO.

## v0.4.0

- Switched the runtime UI from landscape 320x240 to native portrait 240x320 (`TFT_ROTATION=0`).
- Reflowed chrome, six-row lists, 3x3 launcher grid, hint area, softkeys, popup menus and text keyboard for portrait.
- Added an S60-inspired Idle/Home screen with pixel wallpaper, large clock/date, status information and quick-launch tiles.
- Added global keypad shortcuts: hold MENU=Home, OPTION=Settings, START=Music, SELECT=WiFi; B=Back and short SELECT=OK outside text entry.
- Added reusable two-button Symbian dialog and converted File Manager delete confirmation to Delete/Cancel navigation.
- Added a low-memory screen wipe transition between applications.
- Enabled external I2S audio by default on non-conflicting GPIO4/1/2, intended for MAX98357A/PCM5102-style hardware.
- Upgraded WAV playback to PCM16 mono/stereo 8-48kHz, mono-to-stereo expansion, software volume, data-chunk bounds and DMA clearing.
- Added persisted audio volume to Settings.
- Added `tools/test_v04.py`, `docs/UI_V04.md` and `docs/AUDIO_HARDWARE.md`.

## v0.3.0

- Replaced the Launcher list with a classic S60-style 3×3 pixel icon grid.
- Added four-direction keypad navigation with edge wrapping.
- Added D-pad key repeat after a short hold for faster navigation.
- Added an S60-like focus tile and selected-app hint strip below the grid.
- Added direct Clock and System Info launch icons, giving the home menu a full 3×3 layout.
- START and SELECT can both open the focused launcher icon.
- Added return-to-caller behavior for Clock/System Info/About detail screens.
- Kept v0.2 popup menus, pixel icons and list scrollbars intact.

## v0.2.0

- Reworked visual language to more closely match the supplied classic Nokia/Symbian UI reference.
- Added procedural pixel icons for system apps and file types.
- Added reusable vertical scrollbar renderer for long lists.
- Added reusable Symbian-style popup options menu with independent scrolling.
- Added functional Options menus across Launcher, WiFi, BLE, Files, Collection, Music, Settings and Applications.
- Updated softkey labels and classic beige/charcoal palette.
- Kept UI rendering bitmap-free to minimize RAM/Flash overhead.

## v0.1.0

- Initial ESP32-S3/ST7789 shell with WiFi, BLE, Music, Files, Collection, Settings and Applications.

## v0.5.0 - System Services Edition
- Added S60-style Quick panel for WiFi radio, backlight, volume, notifications and keypad lock.
- Added in-RAM Notification Center with a 12-event bounded queue.
- Added keypad Lock screen with clock/date, unread count and 10-second backlight dimming.
- Added configurable automatic keypad lock: Off / 30 s / 60 s / 2 min / 5 min.
- Added persistent Notes app using Preferences and the existing on-screen keyboard.
- Added idle-screen unread notification and background-music status.
- Added shortcuts: Idle OPTION=Quick panel, Idle Up=Notifications, A/B=Lock, hold A=Notifications, hold B=Lock.
- Settings list now scrolls vertically because v0.5 adds Auto keypad lock.
- Fixed duplicate `NimBLEScan *scanner` declaration in the BLE scanner source.
- Kept portrait 240x320 layout and v0.4.1 TFT macro/build fixes.

## v0.6.0 - Core Services & Tasking

- Added `SystemService` with boot count, uptime, minimum-heap tracking and rate-limited low-memory notifications.
- Added a six-entry MRU Recent Apps/task switcher with S60-style long-MENU access.
- Added resume semantics when switching back to an app so list/path/selection state is preserved instead of re-running each app's `enter()` routine.
- Added `WiFiProfileStore` with up to five NVS-backed saved networks, last-network boot reconnect, saved markers and Forget saved action.
- Added Recent Apps to the Applications list.
- Expanded System Info with boot count, uptime, minimum heap, saved network count and alert count.
- Kept v0.5.1 linker isolation (`appCtx` and file-local runtime globals).

## v2.3.3 offline build tooling update
- Added `tools/build_offline.py`, CMD/PowerShell/POSIX launchers with preinstalled PlatformIO cache checks, rejecting HTTP(S) proxy guard, clean target build, SHA-256 and Vietnamese JSON/Markdown diagnostics.
- Added `tools/test_offline_build.py`: 10 deterministic tests with fake PlatformIO/cache (not target compilation).
- No changes to firmware C++/headers, board manifest, GPIO, signed QEAPP/2, or `.vqeaf` rendering.
- Actual target compilation still requires a populated PlatformIO installation and ESP32-S3 toolchain on the machine running the offline builder.

## v2.3.6 — ESP32-S3 icon self-test and target firmware size instrumentation
- Added dedicated ESP32-S3 diagnostic image with 192 frozen v2.3.4 RGB565
  golden CRC cases, real TFT draw timing and RAM diagnostics.
- Production TFT renderer now shares its bounded decoding function with
  on-chip synthetic framebuffer test; immutable RGB565 parity verified on host.
- Added full firmware baseline/optimized PlatformIO build environments and
  a measurement script that does not claim whole-firmware savings until both
  target builds produce real .bin and .elf artifacts.
- Unchanged hardware mapping, portrait layout, vqeaf themes and qeapp format.

## v2.4.3 — Hardware-video-driven installer/theme navigation fix

- Fix: non-deferred START hold no longer invokes global Music after first click already selected an app or theme; MENU is a true release/hold gesture, SELECT retains T9 toggle.
- Fix: fast direct file `.qeapp` inspection; strict absolute path validation; specific error when Snake demo signing key mismatches production firmware; wrapped on-device rejection text.
- Fix: allow direct application of valid `.vqeaf` opened in File Manager outside bounded theme catalog, preserve selection across rescan, do not modify theme/NVS on invalid file.
- Add: `[VQEAF]` serial breadcrumbs, hardware-video-focused fake GPIO/time regression, additional theme/installer path tests, Serial capture script and device retest checklist.
- Scope: host tests and compile/link mocks only; real PlatformIO firmware build/physical device outcome remain unverified.
