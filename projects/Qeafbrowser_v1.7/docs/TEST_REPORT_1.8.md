# Qeafbrowser v1.8 — Smooth Overview Animation Test

Date: 2026-09-23

## Scope

- Fixed-point overview pan easing
- Fixed-point zoom easing (x1..x8)
- Animated preview-page tile cursor
- Animated mini-map/scrollbar position
- RAM regression
- Existing keypad, HTML/WML, JPEG/PNG cache, LittleFS LRU regression

## Implementation

The animation is allocation-free. It stores the visual pan position, visual zoom, animation timestamp, and four fixed-point cursor rectangle values. No off-screen framebuffer, full-screen sprite, or page bitmap was added. Rendering still draws directly to the existing 240x320 display/framebuffer path.

Animation updates at approximately 17 ms intervals only while motion is active. Integer ease-out uses one-third of the remaining distance per tick and snaps when within two fixed-point units.

## Results

- Headless keypad regression: PASS
- qeafivels.com redirect/title regression: PASS
- JPEG/PNG thumbnail/cache regression: PASS
- PSRAM LRU + LittleFS fallback: PASS
- Smooth zoom early/mid/settled capture: PASS
- Smooth pan capture: PASS
- Cross-tile cursor glide capture: PASS

## RAM comparison (Linux simulator, same toolchain)

| Build | text | data | bss |
|---|---:|---:|---:|
| v1.7 | 104184 | 4072 | 277104 |
| v1.8 | 106248 | 4072 | 277136 |

BSS delta: **+32 bytes**. The simulator binary text grew by 2064 bytes; this is code, not runtime framebuffer RAM. ESP32-specific numbers can differ, but the animation state itself is only a few integer variables and booleans.

## Limitation

The ESP32 hardware PlatformIO build was not executed in this environment because the PlatformIO ESP32 toolchain is unavailable here. The shared source was compiled and exercised through the Linux simulator.
