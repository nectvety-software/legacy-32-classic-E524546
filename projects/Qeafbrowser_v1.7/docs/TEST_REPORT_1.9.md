# Qeafbrowser v1.9 — Pixel Smooth Content Scroll Test

Date: 2026-09-23

## Scope

- Pixel-by-pixel main-page scroll animation
- D-Pad focus autoscroll easing
- Desktop Overview -> page handoff synchronization
- Mini-map / scrollbar synchronization
- RAM/BSS regression
- Existing keypad/browser regressions

## Implementation

The content scroll path stores only a visual fixed-point pixel offset, a target fixed-point pixel offset, one timestamp and one active flag. Rendering finds the first visible document line from the visual pixel position and draws the existing document directly to the 240x320 display path. No second framebuffer, page sprite, tile bitmap, or scroll surface is allocated.

The easing cadence is the same ~17 ms integer ease-out used by the v1.8 Overview animation. This keeps movement visually consistent when the user pans in Desktop Overview and presses OK to return to the page.

## Regression results

- Headless keypad/navigation regression: PASS
- D-Pad pixel-scroll progression: PASS (`12 -> 15 -> 19 -> 22 px`)
- Overview -> body pixel-scroll handoff: PASS (`42 -> 53 -> 65 -> 76 px`)
- Final visual position equals target after easing: PASS
- Mini-map / scrollbar move with the visual position: PASS
- No extra framebuffer: PASS

## RAM comparison — Linux simulator, same toolchain

| Build | text | data | bss |
|---|---:|---:|---:|
| v1.8 | 105119 | 4072 | 277104 |
| v1.9 | 106511 | 4072 | 277104 |

The fixed-point scroll state itself is a few scalar values. ESP32-specific linker figures can differ from the Linux simulator.

BSS delta from v1.8: **0 bytes**. Text/code grew by 1,392 bytes in this simulator build; this is code size, not framebuffer RAM.
