#if defined(VQEAF_ICON_BASELINE)
// Frozen v2.3.4 renderer and embedded data. Same full OS, different icon only.
#include "../../docs/verification/v234_baseline/VqeafIconRenderer.cpp"
#else
#include "VqeafIconRenderer.h"
#include "VqeafIconData.h"
#include <TFT_eSPI.h>
#include <string.h>

namespace VqeafIcons {
Palette Palette::standard() {
  return {{
    0x0000, 0x2187, 0xFFFF, 0x065D, 0x22FB, 0xF952, 0xF748,
    0xF524, 0x166C, 0xD71C, 0x532E, 0x9771, 0xF2A9, 0x6DFD
  }};
}
Id fromLegacy(const char *key) {
  if (!key) return Id::Count;
  static const char *const ids[][3] = {
    {"Wi","WiFi",nullptr},{"BLE","BT","Bluetooth"},{"Mus","Music",nullptr},
    {"Dir","Files",nullptr},{"Pic","Gallery",nullptr},{"Web","Internet",nullptr},
    {"Term","Sh","Shell"},{"Rec","Recovery",nullptr},{"Set","Sys","Settings"},
    {"Th","Themes",nullptr},{"App","All","Apps"},{"Col","Library",nullptr}
  };
  for (uint8_t i=0;i<12;++i)
    for (uint8_t j=0;j<3;++j)
      if (ids[i][j] && strcmp(key,ids[i][j])==0) return static_cast<Id>(i);
  return Id::Count;
}
const char *name(Id id) {
  static const char *const names[] = {
    "WiFi","Bluetooth","Music","Files","Gallery","Internet",
    "Shell","Recovery","Settings","Themes","Apps","Library"
  };
  const uint8_t i = static_cast<uint8_t>(id);
  return i<12 ? names[i] : "Unknown";
}

// Shared span decoder: the real TFT renderer AND the ESP32 verification harness
// invoke precisely this function. Asset arrays live only in this translation unit.
// No allocation, no intermediate framebuffer and no flash/SD reads.
using SpanSink = void (*)(void *, uint8_t, uint8_t, uint8_t, uint16_t);

static bool valid(const VqeafIconData::Asset &a, uint8_t size) {
  if (a.palette_len == 0 || a.palette_len > 31 || a.bytes == 0 ||
      a.w == 0 || a.h == 0 || a.x0 + a.w > size || a.y0 + a.h > size)
    return false;
  if (a.codec == VqeafIconData::CODEC_NIBBLE4)
    return a.palette_len <= 16 && a.bytes == (uint16_t(a.w) * a.h + 1) / 2;
  return a.codec == VqeafIconData::CODEC_RLE5;
}

static bool decode(const VqeafIconData::Asset &a, SpanSink sink, void *ctx) {
  const uint16_t total = uint16_t(a.w) * a.h;
  uint16_t pixel = 0;
  uint16_t cursor = 0;
  if (a.codec == VqeafIconData::CODEC_NIBBLE4) {
    for (uint8_t row = 0; row < a.h; ++row) {
      uint8_t col = 0;
      while (col < a.w) {
        if ((cursor >> 1) >= a.bytes) return false;
        const uint8_t raw = a.rle[cursor >> 1];
        const uint8_t role = (cursor & 1) ? (raw & 15) : (raw >> 4);
        if (role >= a.palette_len) return false;
        uint8_t len = 1;
        while (col + len < a.w) {
          const uint16_t next = cursor + len;
          if ((next >> 1) >= a.bytes) return false;
          const uint8_t nextByte = a.rle[next >> 1];
          const uint8_t other = (next & 1) ? (nextByte & 15) : (nextByte >> 4);
          if (other != role) break;
          ++len;
        }
        if (role)
          sink(ctx, uint8_t(a.x0 + col), uint8_t(a.y0 + row), len,
               a.rgb565[role]);
        cursor += len;
        col += len;
      }
    }
    return cursor == total;
  }

  while (pixel < total) {
    if (cursor >= a.bytes) return false;
    const uint8_t token = a.rle[cursor++];
    uint8_t role;
    uint16_t len;
    if (token == 0xF8) {
      if (cursor >= a.bytes) return false;
      role = 0;
      len = uint16_t(8) + a.rle[cursor++];
    } else {
      role = token >> 3;
      len = (token & 7) + 1;
    }
    if (role >= a.palette_len || len > total - pixel) return false;
    if (!role) {
      pixel += len;
      continue;
    }
    while (len) {
      const uint8_t row = uint8_t(pixel / a.w);
      const uint8_t col = uint8_t(pixel % a.w);
      const uint8_t count = uint8_t((len < a.w - col) ? len : (a.w - col));
      sink(ctx, uint8_t(a.x0 + col), uint8_t(a.y0 + row), count,
           a.rgb565[role]);
      pixel += count;
      len -= count;
    }
  }
  return cursor == a.bytes;
}

struct DrawDestination { TFT_eSPI *tft; int16_t x, y; };

static void drawSpan(void *ctx, uint8_t x, uint8_t y,
                     uint8_t w, uint16_t rgb565) {
  DrawDestination &d = *static_cast<DrawDestination *>(ctx);
  d.tft->drawFastHLine(d.x + x, d.y + y, w, rgb565);
}

bool draw(TFT_eSPI &tft, Id id, int16_t x, int16_t y, uint8_t size,
          uint16_t background, const Palette &palette, bool clearBackground) {
  const uint8_t i = static_cast<uint8_t>(id);
  if (i >= 12 || (size != 24 && size != 36)) return false;
  const VqeafIconData::Asset &a = size == 24 ? VqeafIconData::ICONS_24[i]
                                           : VqeafIconData::ICONS_36[i];
  if (!valid(a, size)) return false;
  (void)palette; // Per-icon RGB565 is intentionally immutable.
  if (clearBackground) tft.fillRect(x, y, size, size, background);
  DrawDestination dest = {&tft, x, y};
  return decode(a, drawSpan, &dest);
}

#if defined(VQEAF_ICON_SELFTEST)
bool decodeSpansForTest(Id id, uint8_t size,
                        PixelSpanCallback callback, void *user) {
  const uint8_t i = static_cast<uint8_t>(id);
  if (i >= 12 || (size != 24 && size != 36) || callback == nullptr) return false;
  const VqeafIconData::Asset &a = size == 24 ? VqeafIconData::ICONS_24[i]
                                           : VqeafIconData::ICONS_36[i];
  return valid(a, size) && decode(a, callback, user);
}
#endif

void focusFrame(TFT_eSPI &tft, int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t outer, uint16_t inner) {
  if (w < 6 || h < 6) return;
  tft.drawRect(x, y, w, h, outer);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, inner);
}
} // namespace VqeafIcons
#endif // VQEAF_ICON_BASELINE
