# VQEAF OS 2.5.1 — QEAPP launch, RGB565 app icons, memory and measured performance

Release type: **PC-verified firmware candidate**. No unverified claim of physical ESP32-S3 FPS or that all QEAPP packages can run.

## 1. Which `.qeapp` files can actually run?

Current **production** firmware understands only **signed QEAPP/2** `type=web` and `type=text`. A `web` package opens its signed `entry=https://...` via Qeafbrowser (needs WiFi for a live page). A `text` package opens its already-verified installed `payload.txt` in Text Viewer. **Pixel Snake is a special built-in C++ handler** requiring the sample package signed for the separate `vqeaf_snake_demo` firmware trust profile. It does not imply an independent Lua/C++ runtime. QEAPP Studio beta Lua game packages cannot run on stock production firmware yet. Never turn off signature verification to make an untrusted package run.

**Trust profile:** `sd/System/Apps/Inbox/welcome.qeapp` and `help_site.qeapp` are signed for the normal example production trust key. `games/pixel_snake/dist/snake_pixel_demo.qeapp` uses the *demo* trust profile. Always flash the matching profile in a safe test device. A package signed by another publisher/key must remain rejected.

## 2. Fixes and implementation

1. `AppInstallerService::inspectWithIcon`: verify the chosen package exactly once before displaying its preview. The UI reuses the **same already-verified open file** to obtain the 2048-byte icon. Actual installation verifies again independently before writing.
2. Successful installer result screen offers **Open** on START. A launch re-verifies the entire installed signed receipt and all package content. Any rejection now emits `[VQEAF][QEAPP][LAUNCH_FAIL] id=... reason=...` and a visible UI error, instead of appearing to open the wrong app.
3. `AppInstallerService::refreshIfNeeded`: applications menu/navigation does not recursively re-hash the complete installed app catalog on every visit. Explicit Rescan, signed install/update/uninstall and microSD remount refresh the catalog as before. Any launch **always** rechecks signature and installed bytes.
4. App icon displays read 32×32 RGB565 **little-endian** files. Central `QeappIconBlit.h` sets TFT_eSPI `swapBytes(true)` just for the draw and restores the previous value. All app icon drawing sites now use this one path; list rows place icons *inside* 42-pixel cells without overlapping the next row. Fallback raster badges are skipped if a signed custom icon is drawn.
5. On catalog refresh, the already-verified signature receipt's icon SHA256 is cached per entry (32 bytes). `loadIcon` reads and hashes **only the 2 KiB icon**, rejects post-refresh tampering, and avoids full 256 KiB re-verification on every highlighted row. **A launch still full-reverifies.** Explorer's one-icon cache now invalidates when catalog revision changes.
6. Applications and App Installer share one 2 KiB icon scratch buffer instead of two: **~2048 bytes static saved**, offset by **13×32 = 416 bytes** of hash slots (12 apps + one unused sentinel) plus a few catalog fields. These are source-level static estimates, not a physical heap measurement. No full-screen 153,600-byte RGB565 framebuffer is added.
7. For heavy navigation among Apps, Installer, Browser and Text Viewer, skip the obsolete stripe-out animation and 32-ms opening page before the destination's own draw. Menu/Idle styling and other transitions remain unchanged. Diagnostic `vqeaf_perf_nav_baseline` restores the *old transition policy* on the **same v2.5.1 code** for an apples-to-apples A/B on device. It is NOT an untouched v2.5.0 firmware build.

## 3. Firmware and benchmarks (on the actual ESP32-S3)

Build normal production and upload:

```powershell
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
```

For metrics on stock signed web/text apps (or use `tools\RUN_V251_DEVICE_BENCHMARK.bat COM5`):

```powershell
pio run -e vqeaf_perf_diag -t upload
py -3 -m pip install pyserial
py -3 tools/measure_v251_serial.py --port COM5 --baud 115200 --seconds 120 --interactive --out build_reports/device/v251_prod
```

For Pixel Snake **only on a test device**, flash matching demo-key profiler, place demo QEAPP in `System/Apps/Inbox` and install/open it:

```powershell
pio run -e vqeaf_perf_snake_demo -t upload
py -3 tools/measure_v251_serial.py --port COM5 --seconds 120 --interactive --out build_reports/device/v251_snake
```

Use `i` + Enter **just before installing**; use `b` + Enter before the timed game/navigation pass; use `q` to stop. Do not open `pio device monitor` and this script concurrently: only one process owns COM5. Opening USB CDC may cause one initial reset; mark the *actual* install before associating any later reset with it. If the device disappears during a crash, save the partial log and use `tools/capture_install_reset.py` (auto-reconnect) to investigate first.

Instrumented firmware prints one `[VQEAF][FPS]` and `[VQEAF][PERF]` line about every 5 s. The parser saves:

- `serial_raw.bin`, `serial_timestamped.log`: original bytes and timestamped text; **review for private WiFi credentials before sharing**.
- `fps_windows.csv`, `loop_windows.csv`, `metrics.json`, `report.md`, `events.csv`.
- `game_effective_fps`: FPS of **actual rendered Pixel Snake updates**, not LCD refresh rate and not a guarantee of 60 FPS; the game intentionally advances only once per chosen simulation tick.
- `game_draw_avg_us`, `nav_avg_us`, `input_dispatch_avg_us` and approximate upper-bound p95 buckets: software time from entry into draw/dispatch to return. This **excludes hardware GPIO edge-to-photon latency**. Physical latency requires photodiode/high-speed camera + controlled key timing.
- `heap8`, `largest8`, `psram`: minimum values observed in 5-s snapshots, not guaranteed absolute watermarks.

Use similar workloads for A/B: repeated app opening and switching, the same theme, the same storage card and the same Pixel Snake mode. For transition-only A/B, flash `vqeaf_perf_nav_baseline` and `vqeaf_perf_diag` on separate passes, then compare their `nav_avg_us`, max and percentile while keeping the same key sequence. The tool does **not** invent numerical device results without an attached device.

If you already captured a text log:

```powershell
py -3 tools/measure_v251_serial.py --input serial_timestamped.log --out build_reports/device/from_log
```

An imported log is marked **not independently verified as physical hardware**. The test parser sample is synthetic and must never be published as measured FPS.

## 4. Functional on-device acceptance

1. From fresh boot, `Applications` > `App installer` > install `welcome.qeapp` > use the result-screen `Open`: should show bundled text and correct RGB565 icon.
2. Back to Applications; scroll all installed rows; selected-row color and icon must stay correct without a flash-overlap into the next row.
3. Open `help_site.qeapp` **with WiFi connected**. If it cannot load, note Qeafbrowser HTTPS error: that is distinct from installer/signature failure.
4. Remove/reinsert microSD, run `Rescan` and reopen; app catalog and Explorer preview should refresh their icons.
5. Tamper with a signed app icon on a *test SD copy*: icon load and launch must reject, not display attacker-supplied artwork.
6. Observe Serial during installs. No unsolicited reboot, stack panic, watchdog, or missing `INSTALL` end marker. Do not infer a reset cause without actual device logs.
7. Compare performance with WiFi/SD workloads controlled, logging at least 2 minutes. Capture min free SRAM, largest free block, free PSRAM, loop latency, game draw/update FPS and nav/input dispatch latency.

## 5. Tests and limitations

`python3 tools/verify_v251.py` compiles actual GNU++11 firmware on PC shims (both normal and opt-in diagnostics), runs independent byte-order tests, full ECDSA install/tamper/rollback gates, v2.5.0 UI RGB565 parity, reset regression and Serial parser synthetic tests. `--full` additionally runs extensive legacy app tests. These do **not** replace a real PlatformIO build, ST7789 color confirmation, or physical on-device FPS.

The system is a feature-phone-style **embedded application launcher**, not Symbian S60 binary compatible. It cannot execute `.sis`, `.sisx` or arbitrary downloaded native code. Separate QEAPP Studio Lua runtime integration is ongoing and needs its own signed package format/verified dispatcher and hardware validation.
