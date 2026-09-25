# Symbian S3 OS v1.6.0 – Qeafbrowser-style WiFi Wizard

**Hardware:** legacy-32-classic-E524546, ESP32-S3 N16R8, ST7789 **240×320 portrait** with D-pad/softkeys. No SIM modem: status bar remains centered clock + close-packed WiFi/battery only.

## Design and source

This in-OS WiFi application adapts the workflow of `projects/Qeafbrowser_v1.7/src/main.cpp` in `nectvety-software/legacy-32-classic-E524546`: keep a working WiFi connection while scanning, scan visible+hidden networks, strongest RSSI first, choose using keypad, use password editor, show connection progress and an explicit result after at most 12 seconds. Browser, Shell and WiFi app all use the **same OS WiFi station/radio**; Qeafbrowser is not embedded as a separate firmware UI.

The reference firmware stores `cfg_ssid/cfg_pass` in `/Qeafbrowser/config.ini`. The combined Symbian OS instead uses the **existing** `symbian-net` Preferences/NVS profile store (up to five profiles). It calls `WiFi.persistent(false)` to stop the framework independently writing unsuccessful passwords to flash and intentionally does **not** duplicate the plaintext WiFi password onto the SD card or auto-create a second browser config. The BrowserService automatically uses the live station connection.

## Menu and behavior

1. **Menu → WiFi** shows `Search WLAN` while asynchronous scan runs, then up to 16 access points sorted by RSSI (strongest first). Hidden SSIDs are marked `<hidden>`; choose one to type its SSID manually. No `WiFi.disconnect()` is executed merely by scanning or rescanning.
2. Up/down select networks, START/SELECT invokes Connect. Saved networks reuse their NVS password; open networks connect directly; encrypted/hidden networks invoke the standard 6-column **on-screen keypad** (D-pad navigation, START insert, OPTION Shift, B delete, MENU Done, A cancel). This is the existing in-OS keypad editor, rather than a bit-for-bit copy of the standalone Qeafbrowser multi-tap implementation.
3. Progress updates **once each second in a small area**, never block the main `loop()`. The connection uses a 12-second timeout and reports errors (no SSID, rejected key, timeout) in an S60 message screen. Back cancels explicitly; opening another app allows the connection attempt to finish without stopping the OS loop.
4. **Options:** Connect, Rescan, Disconnect, Forget saved, Network status, Saved networks, Hidden network, Network list. `Forget saved` removes the chosen NVS profile (and disconnects only if explicitly forgetting the current SSID).
5. Credentials are saved *only after verified `WL_CONNECTED` to the requested SSID*. Selecting an already-connected SSID with *exactly the same trusted saved credentials* preserves the association. Replacing its password forces a real reconnect instead of treating the existing session as proof that the replacement works.
6. `Settings → WiFi auto reconnect` and Safe Mode constrain auto-reconnect after manual joins. Intentional `Disconnect` temporarily disables reconnect. The next explicit Connect reenables it only if the setting allows it.

A user-initiated **join of a different AP** must interrupt the previous association; only passive scan is guaranteed not to *explicitly* disconnect an already-running session. WiFi hardware scan may still momentarily delay throughput; avoid scans during large browser downloads.

## Memory and security

- Fixed `Network[16]` table, `char ssid[33]` per network; insertion keeps the top 16 among *all* scan results. Scan driver result memory is released with `WiFi.scanDelete()`.
- No full-screen framebuffer, WiFi data cache, or WiFi background FreeRTOS task added.
- WiFi credentials are never printed in UART logs. Password strings are cleared when the attempt finishes. **NVS is not encrypted by this firmware**; enable ESP32 flash encryption for production devices requiring credential confidentiality.
- WiFi success only verifies association and normal WiFi connection status; it does not establish Internet reachability or server TLS trust.

## Test

```powershell
python tools/test_v16_wifi.py
python tools/test_v14_build.py
pio run -t clean
pio run
pio run -t upload
pio device monitor -b 115200
```

Host tests exercise 12 simulated RF events (incl. active-connection scan retention, strongest 16, hidden networks, credential persistence and rollback, invalid replacement credential, timeout and scan failure). Full GNU++11 host link is separate from the final ESP32/PlatformIO target build and real board validation.
