#pragma once
// VQEAF OS pixel sprites from the user-provided icon artwork. Flash RGB565-RLE.
// TFT_eSPI coordinates are absolute portrait 240x320 (same driver as v2.3).
#include <stdint.h>
class TFT_eSPI;
namespace VqeafIcons {
enum class Id : uint8_t {
  WiFi, Bluetooth, Music, Files, Gallery, Internet,
  Shell, Recovery, Settings, Themes, Apps, Library,
  Count
};
// Role order matches the build_icons.py pixel-token dictionary.
struct Palette {
  uint16_t colors[14];
  // Legacy API (retained). v2.3.4 bakes original art colors per icon; .vqeaf
  // still controls each tile background and the focus frame, not pixel art.
  static Palette standard();
};
// Returns Id::Count for a non-system/legacy icon: keep your existing fallback.
Id fromLegacy(const char *key);
const char *name(Id id);
// Supported sizes EXACTLY 24 or 36. Both generated directly, NEVER scaled at runtime.
// A false return means caller requested invalid size/id; nothing was drawn.
// If clearBackground=true the icon's 24x24 or 36x36 cell is erased before its
// transparent regions are skipped; this also avoids stale glyph pixels on focus.
bool draw(TFT_eSPI &tft, Id id, int16_t x, int16_t y, uint8_t size,
          uint16_t background, const Palette &palette, bool clearBackground = true);
// Fast path for a FLAT, fully opaque 24/36px cell. The caller must pass the
// exact local background; it is written only inside the icon square. Pixel
// colors and geometry are lossless relative to draw(...,clearBackground=true).
// A 36-pixel scanline (72 B stack) replaces hundreds of individual SPI spans.
// Do not use when transparent art must preserve a non-uniform underlay.
bool drawOpaque(TFT_eSPI &tft, Id id, int16_t x, int16_t y, uint8_t size,
                uint16_t background);

// Test-only access to the SAME optimized decoder used by draw(). No icon
// data copied into the test TU, so normal firmware Flash stays unchanged.
#if defined(VQEAF_ICON_SELFTEST)
using PixelSpanCallback = void (*)(void *, uint8_t, uint8_t, uint8_t, uint16_t);
bool decodeSpansForTest(Id id, uint8_t size, PixelSpanCallback sink, void *context);
#endif
// Unified high-contrast focus outline on a parent tile, not around each glyph.
// Call on tile BEFORE draw() and after filling selection background.
void focusFrame(TFT_eSPI &tft, int16_t x, int16_t y, int16_t width,
                int16_t height, uint16_t outer, uint16_t inner);
} // namespace VqeafIcons
