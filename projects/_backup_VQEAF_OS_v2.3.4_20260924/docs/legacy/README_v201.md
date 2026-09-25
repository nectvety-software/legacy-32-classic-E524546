## v2.0.1 board diagnostics (physical validation kit)

Instrumented serial/onscreen tests for live verified TLS and idle microSD remove/reinsert. **On-board tests are pending** until you run them on the connected device; see `docs/BOARD_TEST_V201.md`.

# Symbian S3 OS v2.0.0 — Reliable microSD & Verified HTTPS

**ESP32-S3 N16R8 / ST7789 240x320 portrait**, Nokia-inspired S60 UI.
All existing apps, WiFi profiles, themes and signed QEAPP/2 support retained.

**microSD:** read-only card-health probe every 8s while no WAV is playing or
paused; on detected loss the OS invalidates its mounted flag, issues a
notification, and retries initialization on a 5/15-second schedule without
rebooting. Filesystem directory layout is restored on reinsertion and the
installed app catalog refreshed. There is **no dedicated card-detect GPIO**
in the documented E524546 wiring, so physical hot-removal detection is
best-effort and must be verified on your PCB. Do not pull a card while writing.
`StorageService::writeAtomic` uses `.tmp/.bak` and now **reads back staging
bytes** before replacing previous content. `sd` in Shell shows mount/I/O
state. No full filesystem journaling or hardware power-fail guarantee is claimed.

**HTTPS:** Browser and Shell downloads use `WiFiClientSecure::setCACert()`
with **seven embedded Mozilla trust anchors** rather than `setInsecure()`;
they require a plausible NTP/system time. HTTPS pages block HTTPS-to-HTTP
redirect downgrade. Downloads are **HTTPS-only**, reject redirects to HTTP
and require `Content-Length` so the completed `.part` can be checked for
truncation before publication. The Browser uses distinct v2.0 cache names so
previously fetched pages from the insecure v1.9 path are not trusted. HTTP
plaintext *browsing* is still possible for legacy websites and is not secure.
The seven-root built-in list is intentionally limited: sites chained to other
roots will fail until the firmware trust store is updated. Root CAs cannot
be silently added by replacing files on removable SD.

Run: `python tools/test_v20_storage_tls.py` (includes full previous regression
suite). Use PlatformIO on the machine connected to the actual board:

```powershell
pio run -t clean
pio run
pio run -t upload
pio device monitor -b 115200
```

**Host tests are not evidence of a successful target build, real HTTPS
handshake, or safe hot-swapping on the physical board.** Full limitations,
trust anchor fingerprints and device smoke tests are in
[`docs/TLS_SD_V20.md`](docs/TLS_SD_V20.md).

---

# Symbian S3 OS v1.9.0 — Stability / Storage / Browser

**ESP32-S3 N16R8 · ST7789 portrait 240×320 · keypad**. This is a
firmware operating environment, **not Linux or a native .vxp runtime**.

v1.9 addresses confirmed code-level faults: HTTP response hangs and partial
pages, unsafe URL truncation/relative resolution, cache replacement data loss,
partial cache cleanup, corrupted WiFi NVS profiles and redundant profile flash
writes, the heavy-font overlay repaint issue, and the misleading unsynced
standby clock. The signed QEAPP/2 installer, Theme Manager, Gallery, BLE, Music,
Calculator, Stopwatch, Shell, and signal-prioritized WiFi remain included.

See [`docs/STABILITY_V19.md`](docs/STABILITY_V19.md) for what is fixed,
reproducible host tests, and **remaining hardware/security limitations**.
The old version-specific structural tests in `tools/test_v*.py` are preserved
for historical comparison; several assert *obsolete exact implementation
strings* and are not all valid gates for current releases. For the current
release use `python tools/test_v19_stability.py`.

**Hardware verification remains necessary.** The local environment has no
PlatformIO target toolchain or physical E524546 board. Build and flash on your
host with `pio run -t clean && pio run && pio run -t upload`, then monitor UART at
115200 baud. A desktop host-link PASS is not a target firmware PASS.


## v1.8 — Utilities and media upgrade

This build keeps the ESP32-S3 N16R8 ST7789 portrait 240x320 configuration and
all v1.7 saved WiFi auto-selection behavior. Added **Applications > Calculator**
(4x4 S60 keypad with D-pad navigation, START to enter, OPTION for decimal/backspace/clear,
clear and overflow/divide-by-zero handling) and **Applications > Stopwatch**
(centisecond display, Start/Pause, four laps, paused-only reset). Both keep
state when switching tasks and work in Safe Mode. Stopwatch uses millis() and
therefore measures elapsed time rather than wall-clock time (and wraps
correctly across one millis rollover for intervals under 49 days).

**Music:** a full Now Playing page, selection-aware play/pause, previous/next,
shuffle and repeat Off/All/One, volume +5/-5 on D-pad when viewing Now Playing,
seconds/progress display, bounded 32-track shuffle cycle, and background playlist advances after normal WAV
completion. The I2S output waits ~150ms before clearing DMA at end of file to
avoid truncating its final audio buffer. WAV requirements stay **16-bit PCM,
mono or stereo, 8–48 kHz**, with the external I2S DAC on the BoardConfig pins.

**Gallery:** Slideshow options (four-second steps), Back-to-list and direct
viewing of BMP/JPEG/PNG opened from arbitrary File Manager folders. **BLE:**
top 16 advertisements by measured RSSI (including >16 result scans) and a
sticky Details page with Back; it remains a scanner, not a GATT client.

Tests (host simulator/model only): `python tools/test_v18_core.py`, then
`python tools/test_v17_auto_wifi.py` and `python tools/test_v14_build.py`.
The PlatformIO target still needs a real build and board smoke testing.

## v1.7 — Saved-network auto selection on boot and live S60 standby status

- **Nonblocking boot scan** considers every network in `symbian-net` Preferences/NVS, sorts matching saved SSIDs by measured RSSI and attempts the strongest first. The last successfully used SSID wins only when RSSI ties; it is *not* blindly preferred over a stronger network.
- Fallback: each saved AP discovered in the same scan is attempted at most once, in signal order. Wrong password, lost SSID or 12-second timeout advance to the next candidate. Exhausted/empty scans retry after 30, 60, then 120 seconds maximum; disconnection after a working session is debounced for five seconds before scanning again.
- Safe Mode and `Settings > Auto WiFi strongest` suppress boot scanning. User `Disconnect` / radio-off does not immediately undo the user's choice. Explicit Shell `wifi` controls also suspend the selector to prevent radio-operation races. Merely opening and closing WiFi Manager does not permanently disable recovery.
- Idle/Home shows `scanning saved`, `joining <SSID>`, `connected <SSID> RSSI` or retry countdown. Only the small WiFi line and right-side WiFi/battery glyphs are repainted; 240×320 layout, centered clock, themes and other apps remain intact.
- A successful association updates last-used SSID without needlessly rewriting every NVS profile. Saved passwords are not copied to SD or written to UART.
- See `docs/WIFI_V17_AUTOCONNECT.md`, `tools/test_v17_auto_wifi.py`; hardware RF/PlatformIO validation still required.

## v1.6 — WiFi Wizard synchronized with Qeafbrowser

- Non-blocking 2.4 GHz WLAN scan including hidden SSIDs; selects the 16 strongest APs from the *entire* scan without disconnecting a working Qeafbrowser session.
- Keypad wizard: choose network, enter WPA password, 12-second join status/result, status/details, saved network list, forget, hidden SSID entry, rescan and explicit disconnect.
- Five existing NVS profiles retained; credentials are written only on successful association to the intended SSID; no credentials in UART logs or new SD plaintext file. In-OS Browser and Shell share the WiFi station.
- Per-second progress redraw without blocking browser/music/system shell or allocating a full framebuffer.
- See `docs/WIFI_V16.md` and `tools/test_v16_wifi.py`. Target PlatformIO build/physical RF check required.

## v1.5 — Signed QEAPP Installer

ESP32-S3 N16R8, ST7789 240×320 **portrait**, Nokia-inspired S60 UI.

**Security update:** only cryptographically signed **QEAPP/2** packages can
be installed. Firmware pins a single 65-byte ECDSA P-256 public key in
`src/services/QeappTrustKey.h`; the private signing key is **not present** in
the firmware, microSD contents, examples or released ZIP. Two included sample
apps are signed with the bundled demonstration public key. To distribute YOUR
own apps, provision your own publisher private/public pair locally with
`tools/qeapp_keys.py` and rebuild the firmware. See
[`docs/QEAPP_V15_SIGNING.md`](docs/QEAPP_V15_SIGNING.md) for required steps.

The installer verifies SHA-256 section hashes and an ECDSA P-256 signature
before enabling **Install**. Signature failures are shown in Package rejected
view. Installs use a staged directory and a signed receipt, which is rechecked
against installed bytes at boot and again before app launch. Tampered apps
are omitted from the trusted catalog; restore from your original signed app.
Legacy unsigned QEAPP/1 is rejected and must be re-packaged and re-signed.
No native or downloaded executable code is enabled by this update.

Test on host:

```sh
python3 -m pip install cryptography
python3 tools/test_v15_signature.py
python3 tools/test_v14_build.py  # Arduino API mock; NOT ESP32 target validation
```

Actual target build/flash:

```sh
pio run -t clean && pio run
pio run -t upload
pio device monitor -b 115200
```

---

# Previous release details and hardware notes

# Symbian S3 OS v1.3.0

A Nokia/Symbian S60-inspired handheld OS shell for the `legacy-32-classic-E524546` ESP32-S3 board.

## Target hardware
- ESP32-S3-WROOM-1 N16R8: 16 MB Flash, 8 MB PSRAM
- ST7789 2.0-inch TFT, **portrait 240x320**
- 10 physical keypad buttons
- microSD using the board pin map
- Optional external I2S DAC/amplifier: GPIO4 BCLK, GPIO1 WS, GPIO2 DOUT

The board GPIO mapping remains in `include/BoardConfig.h`.




## v1.3 – Nokia-like bold UI + SD system layout

- UI labels now use a **Nokia 2700/S40-inspired heavy raster style**: TFT_eSPI's
  built-in fonts are overdrawn by one pixel instead of loading a large custom
  font. Titlebar, softkeys, grid labels, list titles, popup menus and dialogs are
  heavier while preserving the 240×320 layout and RAM budget.
- Mounting microSD automatically creates a stable OS layout:

```text
/System/
  Cache/Web/        # Qeafbrowser HTML cache (16 files / 512 KiB cap)
  Cache/Thumbs/
  Themes/           # preferred .vqeaf location
  Apps/Installed/
  Apps/Inbox/       # downloaded .qeapp/.zip/.vxp packages
  Downloads/        # ordinary browser downloads
  Logs/
  Temp/
/Media/Music/
/Media/Pictures/
/Documents/
/Themes/             # compatibility with v1.2 cards
```

- Qeafbrowser now keeps a bounded persistent HTML cache on SD. When WiFi is
  unavailable, **Options > Home** can reopen a cached page; a small `C` marker
  appears in the address bar when the page came from cache.
- Browser **Options > Download link** streams the selected URL directly to SD
  using a 512-byte buffer. `.vqeaf` goes to `/System/Themes`, `.qeapp/.zip/.vxp`
  to `/System/Apps/Inbox`, and other files to `/System/Downloads`. Downloads are
  capped at 4 MiB in this firmware profile.
- Applications contains **App inbox**, and File Manager Options has direct
  shortcuts to Downloads, Themes and App inbox. Downloaded packages are staged
  data; v1.3 deliberately does not execute arbitrary binaries from SD.
- Music, Gallery and Text Viewer prefer `/Media/Music`, `/Media/Pictures` and
  `/Documents`, while retaining root-card fallback for older cards.
- Shell adds `layout` plus `cache status|prune|clear`; `df` also reports free SD
  space.

See `docs/SD_STORAGE_V13.md` and `docs/BROWSER_DOWNLOADS_V13.md`.


## v1.2 – Themes application (microSD import)

- Added a dedicated **Themes** icon to the 240×320 S60 3×4 launcher and
  **Applications > Themes**; Settings > Theme opens the same app.
- Put `.vqeaf` files created in VQEAF Theme Studio in **`/Themes/`** on your
  microSD. The app checks this folder first, then searches the card root and
  subfolders (bounded depth, maximum 16 results). It also works with no SD:
  the four built-in themes remain selectable.
- Use **D-pad Up/Down** to highlight a theme and **START/SELECT** to apply it.
  **OPTION** offers Apply, Rescan microSD and Theme details; A/B = Back.
  From File Manager you can also press Open on a `.vqeaf` to jump into Themes.
- `.vqeaf` v1.x colors are read into a staged fixed-size RGB565 palette.
  Supported palette keys: `screen`, `key`, `keyPressed`, `keyBorder`,
  `keyText`, `subText`, `shellTop`, `shellBottom`, `accent`, `glow`,
  plus `panel`, `selected`, `titlebar`, `chromeText`, `border` aliases.
  This device intentionally does **not** render Theme Studio's phone-frame
  shapes, shadows, blur, animated textures or keypad textures: it is a small
  240×320 TFT firmware skin, not a phone-frame emulator.
- Selection is saved in Preferences (NVS), and the selected file is loaded
  again after the SD mounts at the next boot. Missing or damaged files use
  the built-in **S60 Green** appearance without wiping the saved file path.
- Parser limits: at most **16 entries**; at most **32 KiB** per imported file;
  at most **400 lines**, **319 bytes/line**; invalid colors, path traversal,
  partial palettes and unsupported versions are rejected without changing the
  active theme. The source file is read-only, and no `.vqeaf` text is executed.
- No full-screen skin framebuffer. Launcher/menu icon drawing remains
  procedural; only switching a theme performs one complete redraw.

Quick start:

```text
microSD:/
  Themes/
    amoled_red.vqeaf
    s60_green.vqeaf
    custom_from_studio.vqeaf
```

Power off to insert or remove your SD card. Open **Menu > Themes**, pick a
filename, press **Open/Apply**, then choose **Options > Rescan microSD** if
files have changed. Theme editing and SD copying can be done on a PC; the
firmware does not modify the source `.vqeaf` file. Read `docs/THEMES_V12.md`
for format limits and troubleshooting.

## v1.1 Theme Engine / compact status group

- Added built-in **AMOLED Red**, mapped directly from VQEAF Theme Studio `themes/amoled_red.vqeaf`: black screen, #171717 panels, #38161B selection, #8A2E3B borders, white text, #B78E94 secondary text and #FF3D5B accent.
- WiFi and battery glyphs are now grouped at the far-right with a 6 px gap instead of occupying two widely separated 40 px slots. The clock remains exactly centered on the 240 px LCD.
- AMOLED Red keeps the same 3x4 S60 application grid and 36x36 procedural icons, but uses near-black row bands, red focus/glow lines and an AMOLED-friendly standby wallpaper.
- Theme switching remains allocation-free: built-in palettes are RGB565 constants and no full-screen skin bitmap is kept in RAM.
- Device theme source files are included under `themes/` for VQEAF-style editing/reference.


## v1.0.1 S60 title/status alignment

- Keeps the display fixed at **240x320 portrait**.
- Splits the 240px top bar into three equal 80px zones: app title, centered clock, status cluster.
- Centers the clock against the full LCD width instead of anchoring it near the right side.
- Gives WiFi and battery equal 40px slots in the right-hand status zone.
- Vertically centers title, clock and status icons in the 27px S60 titlebar.
- Uses the same geometry on Active Standby/Home and keeps cellular/SIM indicators absent.
- RSSI changes now repaint only the 80px status zone, preserving the anti-flicker renderer.

## v1.0 S60 Green Theme / 240x320
- Added a new **S60 Green** theme inspired by the supplied Nokia/Symbian menu reference. It is now the one-time migrated/default appearance after updating from v0.x.
- The UI remains native **portrait 240x320** (`TFT_ROTATION=0`, `SCREEN_W=240`, `SCREEN_H=320`). No landscape path is introduced.
- Launcher changed from 3x3 to a denser **3x4 S60 grid** with twelve directly accessible apps: WiFi, Bluetooth, Music, File manager, Gallery, Internet/Qeafbrowser, Shell, Recovery, Settings, Notes, Apps and Library.
- Menu icons are larger 36x36 procedural/vector-pixel graphics with simple S60-style highlights/shadows. No menu bitmap is kept in RAM.
- Added a lime-green four-band menu wallpaper and pale selection frame designed so a deselected cell can reconstruct its own background; D-pad navigation therefore keeps the v0.6.1 anti-flicker partial-redraw path.
- Title/status bar is now 27 px and the softkey navbar 22 px in the S60 Green skin. The hardware status area still shows **WiFi + battery only** because this board has no SIM/cellular modem.
- Settings now cycles three skins: **S60 Green -> Classic beige -> Black**, and `Reset appearance` returns to S60 Green.
- The green wallpaper also propagates to Idle/Home and lock UI while list/dialog/popup colors follow the active theme palette.
- Existing v0.9 advanced file commands, network utilities and live monitoring remain unchanged.


## v0.9 File + Network Tools
- Extended the bounded in-OS Shell with file management: `mkdir`, `rm`, `rmdir`, `touch`, `cp`, `mv`, `write`, `append`, and bounded `hexdump`. Paths can be quoted when they contain spaces.
- File copies stream through one fixed 512-byte buffer. `rm` never removes directories, `rmdir` only removes empty directories, and `/` is explicitly protected.
- Added network tools: `nslookup`, `ping`, `wget`, `netmon`, richer `ifconfig`, plus `top` firmware health monitoring.
- `ping` uses the ESP-IDF ICMP session API when available and falls back to a clearly labeled TCP reachability probe when that API is absent.
- `wget` writes directly to microSD through `HTTPClient::writeToStream()` instead of buffering a response body in RAM. HTTPS currently uses `setInsecure()` like the lightweight browser, so server certificates are not authenticated.
- `ifconfig` now shows MAC, IP, mask, gateway, DNS and RSSI. `netmon` shows signal quality, cumulative shell download traffic and the age of the latest network action.
- `top` reports uptime, free/minimum heap, PSRAM, WiFi signal, shell copy bytes and shell download bytes. It is a firmware monitor, not a Linux scheduler/process sampler.
- Shell Options now exposes **Network monitor**, **System monitor** and **File commands** without requiring command typing.
- Command storage remains bounded; the command line limit increases to 127 characters to make URLs and quoted paths practical.

## v0.8 Shell Edition
- Added an in-OS **Shell** application inspired by the console/getty workflow in `platima/esp32-s31-linux`, but implemented natively for this ESP32-S3 Xtensa firmware. The S31 Linux kernel/OpenSBI image is **not** copied or booted on the ESP32-S3.
- Shell keeps bounded output (`20 x 39 chars`) and an eight-entry command history; there is no `fork/exec`, arbitrary binary execution, or unbounded command buffer.
- Commands: `help`, `clear`, `uname`, `version`, `uptime`, `free`, `df`, `mount`, `pwd`, `cd`, `ls`, `cat`, `stat`, `date`, `wifi`, `ifconfig`, `ip`, `ps`, `dmesg`, `safe`, `reboot`, `echo`, `history`.
- `wifi status|on|off|scan` and `wifi <ssid> [passphrase]` mirror the simple command-oriented network control used by the reference Linux project while calling Arduino-ESP32 WiFi directly.
- `safe status|on|off` integrates with the existing crash-recovery state; Shell remains available in Safe Mode for diagnostics.
- Shell uses the S60 titlebar/softkey navbar and the existing on-screen keypad editor. `START` opens command entry, `UP` recalls older commands, and `OPTION` opens Shell actions.
- File commands operate on the mounted microSD and maintain a current working directory. `cat` and `ls` intentionally truncate output to bounded limits to protect RAM.
- Added `/` and `:` to the shared text keyboard so shell paths and URL-like strings can be entered directly.
- Shell participates in **Opening application** and Recent Apps/task resume just like the other S60 applications.

> Reference note: `esp32-s31-linux` targets an ESP32-S31 RISC-V dual-hart platform with OpenSBI/Linux/Buildroot. This project targets ESP32-S3-WROOM-1 N16R8 (Xtensa LX7), so v0.8 adopts its interactive-console ideas rather than pretending the S31 Linux binary stack is compatible.

## v0.7 Recovery / Library / Qeafbrowser integration
- The S60 status area now intentionally shows **only WiFi strength and battery**. The board has no SIM/modem, so there is no cellular/signal/SIM indicator; BLE and microSD state are shown only inside their apps.
- Added **Crash Recovery** backed by Preferences/NVS. The OS records an unfinished boot and the ESP32 reset reason; panic/watchdog/brownout resets increase a crash streak. Two repeated crash boots automatically enable **Safe Mode**.
- Hold physical **A during power-on** to force Safe Mode. Safe Mode does not start I2S audio, BLE, Qeafbrowser, or WiFi auto-connect; File Manager, Gallery, Text Viewer, Settings and Recovery remain available for repair.
- Added **Recovery** application: inspect reset reason/crash streak, boot normal mode, enable Safe Mode, clear recovery flags, or restart. After 12 seconds of stable runtime, the current boot is marked healthy.
- Added **Gallery** for `.bmp`, `.jpg/.jpeg` and `.png`. BMP is streamed row-by-row; JPEG uses TJpg_Decoder; PNG uses PNGdec with the decoder object in PSRAM. There is no 240x320 framebuffer.
- Added **Text Viewer** for `.txt`, `.md`, `.log`, `.json`, `.ini`, and `.csv`. It streams bounded 15-line pages from microSD instead of loading the whole document.
- File Manager routes supported image/text files directly into Gallery/Text Viewer. **Library** remains the media/document category index.
- Integrated an in-OS **Qeafbrowser** adapter based on `projects/Qeafbrowser_v1.7`: HTTP/HTTPS, explicit redirects, Opera Mini-style User-Agent, fixed line/link pools allocated once from PSRAM, small back history, keypad URL entry, link focus, and a 32 KB PSRAM response buffer. It deliberately remains a lightweight HTML/WML-era browser, not Chromium/JavaScript.
- Added S60-style **"Dang mo ung dung"** interstitial when launching an app from Idle, Menu, Applications, Library, File Manager, Quick Panel, or Open applications. Resuming from the task switcher shows **"Dang tiep tuc"** and preserves app state.
- JPEG/PNG dependencies are pinned to the same decoder versions used by the Qeafbrowser source: `TJpg_Decoder 1.1.0` and `PNGdec 1.1.6`.

> The battery symbol is currently a status glyph because the board documentation does not define a battery ADC/fuel-gauge pin. Add the actual battery-sense hardware mapping before displaying a percentage.

## v0.6.1 S60 UI / anti-flicker and memory pass
- Reworked the portrait shell toward classic **S60** proportions: 30 px title/status bar, 24 px softkey navbar, 42 px list rows and 28 px procedural pixel icons.
- Removed full-screen blanking from normal navigation. D-pad changes repaint only the previous/current selection when the list viewport does not scroll.
- Cached titlebar/status and softkey text so unchanged chrome is not resent to the ST7789 on every button press.
- Idle, lock and clock screens update only their clock region; minute-only refresh cadence is 10 s rather than repainting the screen every second.
- On-screen keyboard clears the content area only once when opened; cursor movement redraws in place.
- Transition effect is now a lightweight accent sweep and never blanks the complete LCD.
- Reduced transient/internal RAM pressure: WiFi/BLE result arrays are fixed-size and capped at 16, File Manager cache is 40 entries, Music cache is 32 entries, notifications are capped at 8, and Collection counts files without a temporary 64-entry array.
- Reduced I2S working buffers/DMA queue and releases the NimBLE host/controller after each scan because the current BLE application is scan-only.
- Removed TFT_eSPI Font4 from the build; the UI uses Font2 for S60 titles/softkeys/list labels and Font1 for compact metadata.

These are structural memory optimizations. Exact Flash/DRAM/PSRAM numbers must be taken from the PlatformIO linker/build report on the target toolchain.

## v0.6 core/system logic
- **Recent Apps / Task Switcher** with a six-entry MRU queue.
- Long `MENU` now follows classic S60 behavior and opens **Open applications**.
- Returning from the task switcher uses a **resume path**: WiFi/BLE scans, File Manager path, selected track and list positions are not reset.
- **System health service** tracks boot count, minimum free heap and uptime.
- Bounded low-memory alert: one notification below 48 KB heap, re-armed after recovery above 64 KB.
- **Saved WiFi profiles**: up to five SSID/password profiles in NVS, saved-network marker, Forget saved action and boot auto-connect to the last profile.
- System Info now reports boot count, uptime, minimum heap, saved WiFi count and notification count.
- Previous Quick Panel, lock screen, notifications, Notes, audio, File Manager and S60 UI remain active.

> WiFi passwords are stored in ESP32 Preferences/NVS. They are not cryptographically protected unless flash/NVS encryption is enabled in the firmware/device configuration.

## v0.5.1 linker hotfix retained
- Runtime objects in `main.cpp` remain file-local (`static`).
- `AppContext` remains named `appCtx`, avoiding the ESP32-S3 `libnet80211.a` symbol collision with `ctx`.

## Keypad shortcuts
- `MENU`: open launcher; from launcher return to Idle/Home
- **Hold `MENU`: Recent Apps / task switcher**
- Idle `OPTION`: Quick panel
- Hold `OPTION`: Settings
- Hold `START`: Music
- Hold `SELECT`: WiFi
- Hold `A`: Recovery / Safe Mode
- Hold `B`: Lock device
- Idle `UP`: Notifications
- Idle `A/B`: Lock device
- D-pad: move selection / scroll
- `START`/`SELECT`: open/confirm
- `A/B`: back/cancel in normal app UI

## Main applications
- Idle/Home screen with clock, status and three quick-launch tiles
- 3x4 S60 launcher with S60 Green theme
- WiFi manager with saved profiles
- BLE scanner
- Music player for PCM16 WAV through I2S
- File Manager + Library category index
- Gallery (BMP/JPEG/PNG) + streaming Text Viewer
- Qeafbrowser lightweight web application
- Shell with bounded file/network/monitoring tools
- Recovery + Safe Mode
- Settings + Quick Panel
- Notifications + Notes
- Recent Apps / task switcher
- Clock + System Info + About

## Build

```powershell
cd SymbianS3_OS
pio run -t clean
pio run
pio run -t upload
pio device monitor -b 115200
```

Environment: `legacy32_symbian` in `platformio.ini`.

## Tests

```powershell
python tools\test_grid_nav.py
python tools\test_v04.py
python tools\test_v051.py
python tools\test_v07.py
python tools\test_v08.py
python tools\test_v09.py
python tools\test_v10.py
```

See `docs/UI_V10.md`, `docs/SHELL_V09.md`, `docs/SHELL_V08.md`, `docs/UI_V07.md`, `docs/QEAFBROWSER_INTEGRATION.md`, `docs/UI_V061.md` and `docs/AUDIO_HARDWARE.md`.
