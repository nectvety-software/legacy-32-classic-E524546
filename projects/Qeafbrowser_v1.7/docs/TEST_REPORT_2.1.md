# Qeafbrowser v2.1 — Real-time Clock Test Report

## Scope

Adds a real-time footer clock that synchronizes from Internet NTP after Wi-Fi is available, while preserving the low-RAM Opera Mini 4 / Java-Symbian UI.

## Implementation

- NTP is started with `configTzTime()` on ESP32-S3.
- Servers: `pool.ntp.org`, `time.google.com`, `time.cloudflare.com`.
- Default timezone: `ICT-7` (UTC+7 / Viet Nam), configurable as `timezone=` in `/Qeafbrowser/config.ini`.
- Until synchronization succeeds, the footer displays `--:--`.
- After the first sync, ESP32 system RTC/timekeeping continues even if Wi-Fi temporarily disconnects.
- Network restoration is checked every 2 seconds and triggers NTP again when needed.
- Only the 60×18 px center of the footer is redrawn once per second; no second framebuffer, sprite, or timer task is allocated.
- The colon blinks each second to make the live clock visually apparent.

## Regression results

- Headless keypad/browser regression: PASS.
- qeafivels.com redirect/parser/thumbnail/cache regression: PASS.
- Real-time clock harness (`sim/realtime_clock_main.cpp`): PASS.
- Three screenshots captured across successive seconds: PASS.

## Host build memory comparison

| Version | text | data | bss |
|---|---:|---:|---:|
| v2.0 | 107535 | 4072 | 277104 |
| v2.1 | 109501 | 4120 | 277136 |

BSS increase: **32 bytes**. No framebuffer was added.

## Hardware limitation

This environment does not contain the PlatformIO ESP32 toolchain or physical ESP32-S3 hardware, so the `configTzTime()` path was not flashed here. The host simulator uses real system time shifted to UTC+7 and exercises the same footer update logic.
