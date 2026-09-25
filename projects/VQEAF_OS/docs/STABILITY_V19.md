# Symbian S3 OS v1.9.0 — Reliability audit

Hardware: ESP32-S3 N16R8, 16 MB flash, 8 MB PSRAM, ST7789 **240x320
portrait**, E524546 keypad/SD_MMC. All current apps and /System SD paths
from v1.8 remain. The WiFi+pin-only status bar and 3×4 S60 menu are unchanged.

## Confirmed source faults fixed

1. **Browser could hang after headers.** The old loop only stopped for idle
   time *after* it had received some data. An HTTP/HTTPS peer that sent
   headers and then nothing could keep the main UI stalled indefinitely.
   The body stream now has 8-second no-data and 25-second total limits,
   checks known Content-Length, rejects body >=32 KB, partial transfers,
   and HTTP 4xx/5xx. The first-byte timeout is tested with a fake client
   that stays connected but never produces data. This does not eliminate
   blocking *inside* HTTPClient TLS handshake/request calls.
2. **URL truncation and relative query errors.** A URL longer than the fixed
   buffer silently became a different URL; the parser could fold a
   domain-only `?q` into the host and did not handle query-only links. The
   new normalization validates length, strips invalid URL schemes and rejects
   ASCII control chars; relative `..`, domain-only and query-only cases are
   exercised by actual C++ URL tests. The browser is a lightweight HTML/WML
   reader, not a modern JS/CSS browser.
3. **Lost cached data on interrupted rename.** Replacing an existing cache
   file previously deleted the old copy before the new one was renamed.
   Replacement now uses `.tmp` and `.bak`, with rollback if finalization
   fails and a `recoverAtomicFile` read-time repair path. Incomplete `.tmp`
   files are never treated as valid offline pages. This is *best-effort*
   crash recovery using FAT filesystem rename; it is not a transactional
   journal across faulty SD hardware or power failure during FAT metadata IO.
4. **Bounded cache management.** Prune formerly counted only `maxFiles+8`
   list entries. It now iterates the directory, checks a fixed cap of 128
   entries per pass, and returns -1 if it cannot safely reach the quota.
   Shell clear reopens the directory per deletion instead of removing only
   its first 32 entries. Only the recognized cache folders can be cleared.
5. **WiFi profile correctness/NVS wear.** NVS entries with invalid SSID or
   WPA password, duplicates and counts >5 are repaired once. A redundant
   reconnection to the same preferred SSID/password now avoids rewriting
   the NVS profile set. The OS still stores credentials in ordinary NVS,
   *not* NVS encryption; do not store production secrets until flash
   encryption/NVS security is provisioned.
6. **UI glyphs and clock.** Thick font second and third passes are transparent
   so they cannot paint over already drawn pixels with the background.
   Without synchronized NTP, the UI now shows `--:--` and `Date not set`.
   A successful asynchronous sync produces a one-time notification.

## How to test

```sh
cd SymbianS3_OS
python tools/test_v19_stability.py
pio run -t clean
pio run
pio run -t upload
pio device monitor -b 115200
```

`test_v19_stability.py` compiles the **real changed production** C++ modules
against controlled host APIs, runs actual simulated filesystem failure paths,
HTTP response edge cases, WiFi Preferences/NVS, regression model suites and
an OpenSSL ECDSA/installer verification gate when dependencies exist.
It never substitutes for target or board testing.

## Remaining limitations / follow-up priorities

- TLS uses `WiFiClientSecure::setInsecure()` in Browser and Shell downloads.
  A real CA bundle, trusted system time and HTTPS-only app/theme downloads
  should be added before handling authentication or untrusted critical data.
- Browser HTTPClient GET/TLS handshakes are **synchronous**. Background WAV
  audio may underrun during a slow network request; a dedicated bounded audio
  task with coordinated SD I/O requires hardware-level validation.
- Battery indicator is a static glyph; board documentation needs confirmed
  ADC/fuel-gauge wiring before displaying any numerical charge level.
- No hot-unplug remount, SD write endurance benchmark, physical BLE/GATT,
  long-run heap/PSRAM fragmentation test or validated real-device target
  PlatformIO build has been completed by this release.
- `.qeapp` demo key is not a production publisher identity; provision your
  own key and use flash encryption/secure boot before a security-sensitive
  release. Executable native apps and VXP binaries are not supported by the
  existing signed web/text package runtime.
