# Symbian S3 OS v1.7 — Saved WiFi auto selection

Target: **ESP32-S3 N16R8**, ST7789 **240×320 portrait**, S60 themes, no SIM.

## Startup

1. `SettingsStore` and `WiFiProfileStore` load previous settings and up to five saved networks from NVS.
2. `WiFiConnectionService.configureAuto(settings.wifiAuto && !system.safeMode())` schedules an async 2.4 GHz scan; the Arduino main loop never waits for WiFi during boot. It does not use `WiFi.begin(lastSSID)` in setup.
3. Every scanned SSID is compared against all saved profiles. A saved OPEN profile is only considered when the scanned AP is OPEN; similarly SECURED. A candidate is scored by its **best measured RSSI** (higher, less negative = stronger). Scan results are inspected **before** discarding the framework's scan buffers, even if a saved SSID ranks below the top 16 networks shown in the WiFi UI.
4. Candidates are ordered descending RSSI. Last successfully connected SSID wins equal-RSSI ties. One network is attempted at a time with an independent **12s** deadline. On failed association, the next discovered candidate is tried. Success updates `lastSSID` only when it changed; NVS profile passwords are not re-written automatically.
5. If all discovered saved APs fail or no saved AP is found, retry in **30s / 60s / 120s** (maximum). If a working session drops, allow **5s** of transient loss before re-scanning instead of tearing down an active browser session or roaming during downloads.

## Home display and controls

Idle status text changes among `WiFi: scanning saved...`, `WiFi: joining <SSID>`, `WiFi <SSID> <RSSI>dBm`, `WiFi: retry in <N>s`, `WiFi: no saved networks`, `WiFi: auto-connect off` and `WiFi: Safe Mode (manual)`. The top bar keeps its centered clock and tightly grouped WiFi + battery at right. Only a 205×15px status line plus the 45px icon corner is repainted; no fullscreen redraw or extra framebuffer is allocated.

Settings: `Auto WiFi strongest` toggles scanning (default On). Safe Mode never auto-connects. In Quick Panel, radio-off cancels pending attempts and disables the manager until radio-on; an explicit WiFi Disconnect also suppresses the background auto selector. Opening/closing the WiFi list alone restores automatic selection if no explicit action was taken. A manual selection takes precedence even when its RSSI is weaker. Shell commands `wifi on/off/scan/<SSID>` are also explicit radio operations: they suspend boot auto-selection to avoid racing with their direct network calls (until Settings is toggled or next boot).

**Behavioral limitation:** the OS selects the strongest *discovered saved SSID* at startup or following a disconnection. It intentionally does **not** continually roam to a stronger AP while connected, which would interrupt Browser downloads. Hidden APs that do not expose a saved SSID in scan results require explicit connection through the existing Hidden network wizard. RSSI estimates signal strength, not congestion, Internet access or actual throughput. WiFi association alone does not prove Internet connectivity.

## Resource and security notes

No new FreeRTOS task or full-screen RGB565 framebuffer. Auto mode keeps up to five candidate indices (saved-profile slots), five RSSI values, and the existing 16-entry scan list; the ESP32 WiFi driver owns temporary scan buffers, freed with `scanDelete()`. Passwords remain in `symbian-net` NVS (not encrypted unless flash/NVS encryption is enabled for a production board), not microSD or Serial logs. Failures never overwrite known-good saved credentials.

## Tests

```powershell
python tools/test_v16_wifi.py
python tools/test_v17_auto_wifi.py
python tools/test_v14_build.py
pio run -t clean
pio run
pio run -t upload
pio device monitor -b 115200
```

The first three are host-only and verify simulated RF and C++11 mock compilation; they cannot establish real-world 2.4 GHz RF behavior, WiFi driver-specific association outcomes or actual PlatformIO firmware compilation. On-device manual cases: boot with two stored SSIDs at different RSSI, simulate strongest failing, disconnect temporarily for less than five seconds, observe status on Idle, check Safe Mode and radio-off, and confirm Browser browsing is not disconnected by a passive scan.
