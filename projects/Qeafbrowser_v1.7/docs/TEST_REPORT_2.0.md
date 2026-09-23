# Qeafbrowser v2.0 — Fixed-Point D-Pad Inertia Test

## Scope

This regression verifies light inertial scrolling for the main 240x320 browse view without allocating an off-screen framebuffer. The existing fixed-point pixel scroll from v1.9 remains the position system. v2.0 adds only a small fixed-point velocity state and release friction.

## Motion model

- Position: `BODY_FP = 256` fixed-point pixels.
- Animation tick: about 17 ms.
- Initial D-Pad impulse: 1.5 px/tick.
- Hold acceleration: 0.25 px/tick per tick.
- Maximum velocity: 4 px/tick.
- Release friction: velocity x 230 / 256 per tick.
- Stop threshold: 1/20 pixel/tick.
- Extra coast margin: maximum 8 px, only when focus autoscroll was already needed.
- No float math, framebuffer, sprite, or per-frame heap allocation.

## Inertia regression result

Using `https://keypad.test/`, the focus was moved near the bottom and DOWN was held briefly, then released.

| Frame | Scroll px | Target px | Velocity FP | Inertia |
|---|---:|---:|---:|---:|
| Before | 0 | 0 | 0 | 0 |
| D-Pad held | 6 | 22 | 576 | 1 |
| Release | 6 | 22 | 576 | 1 |
| Coast early | 8 | 22 | 517 | 1 |
| Coast mid | 11 | 22 | 416 | 1 |
| Coast late | 14 | 22 | 335 | 1 |
| Settled | 22 | 22 | 0 | 0 |

PASS criteria: position continued monotonically after release, fixed-point velocity decreased monotonically, and the scroll settled exactly at the target with zero velocity and inactive inertia. All passed.

## Regression gates

- Keypad focus/browser headless regression: PASS.
- v1.9 pixel scroll + overview handoff regression: PASS.
- Smooth Overview zoom/pan/tile animation regression: PASS.
- qeafivels.com thumbnail/cache/LRU regression: PASS.
- v2.0 release inertia regression: PASS.

## Host static-memory comparison

Built with the same Linux simulator command and compiler flags:

| Version | text | data | BSS |
|---|---:|---:|---:|
| v1.9 | 107519 | 4072 | 277104 |
| v2.0 | 107695 | 4072 | 277104 |

BSS increase: **0 bytes**. Text/code size increased by 176 bytes. This is a host-simulator comparison, not an ESP32 linker map measurement.

## Hardware note

The source path remains compatible with the ESP32-S3 build configuration and still uses `TJpg_Decoder` + `PNGdec`; this environment does not include the target PlatformIO toolchain or physical E524546 device, so the hardware firmware was not flashed here.
