// v2.3.6: same on-device decompressor core as the production TFT renderer.
// Golden checksums are generated INDEPENDENTLY from frozen v2.3.4 C++.
#if defined(VQEAF_ICON_SELFTEST)
#include "VqeafIconSelfTest.h"
#include "VqeafIconRenderer.h"
#include "../../docs/verification/device_icons/golden_crc_v234.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
#endif

namespace VqeafIconSelfTest {
static uint16_t tile[36 * 36]; // <3 KiB internal RAM, test build ONLY
struct Raster {
  uint8_t side;
  bool outOfBounds;
};

static uint16_t pattern(uint8_t icon, uint8_t side, uint8_t x, uint8_t y) {
  return uint16_t((unsigned(icon) << 8) ^ (unsigned(x) * 23) ^
                  (unsigned(y) * 41) ^ ((unsigned(x) * y) << 2) ^
                  (unsigned(side) << 7));
}
static void rasterize(void *state, uint8_t x, uint8_t y, uint8_t len,
                      uint16_t color) {
  Raster &r = *static_cast<Raster *>(state);
  if (y >= r.side || x >= r.side || len > r.side - x) {
    r.outOfBounds = true;
    return;
  }
  for (uint8_t i = 0; i < len; ++i) tile[unsigned(y) * 36 + x + i] = color;
}
static uint32_t crcTile(uint8_t side) {
  uint32_t crc = 0xFFFFFFFFu;
  for (uint8_t y = 0; y < side; ++y) {
    for (uint8_t x = 0; x < side; ++x) {
      const uint16_t pixel = tile[unsigned(y) * 36 + x];
      for (unsigned byte = 0; byte < 2; ++byte) {
        crc ^= byte ? (pixel >> 8) : (pixel & 255);
        for (unsigned k = 0; k < 8; ++k)
          crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
      }
    }
  }
  return ~crc;
}

unsigned run(TFT_eSPI &display) {
  Serial.println("[ICONTEST] START v2.3.6 cases=192 ref=v2.3.4 format=RGB565-LE");
  unsigned errors = 0, passed = 0;
  uint32_t decodeUs = 0, drawUs = 0;
#ifdef ARDUINO_ARCH_ESP32
  const uint32_t internalBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
  const uint32_t internalBefore = 0, psramBefore = 0;
#endif
  // Negative API behavior must not call the sink or modify the tile.
  Raster bad = {24, false};
  if (VqeafIcons::decodeSpansForTest(VqeafIcons::Id::Count, 24, rasterize, &bad) ||
      VqeafIcons::decodeSpansForTest(VqeafIcons::Id::WiFi, 25, rasterize, &bad) ||
      VqeafIcons::decodeSpansForTest(VqeafIcons::Id::WiFi, 24, nullptr, &bad)) {
    ++errors;
    Serial.println("[ICONTEST] FAIL invalid arguments accepted");
  }

  const VqeafIcons::Palette palette = VqeafIcons::Palette::standard();
  for (uint8_t icon = 0; icon < 12; ++icon) {
    for (uint8_t variant = 0; variant < 2; ++variant) {
      const uint8_t side = variant ? 36 : 24;
      const unsigned index = unsigned(icon) * 2 + variant;
      unsigned localErrors = 0;
      for (uint8_t b = 0; b < 4; ++b) {
        for (uint8_t clear = 0; clear < 2; ++clear) {
          for (uint8_t y = 0; y < side; ++y)
            for (uint8_t x = 0; x < side; ++x)
              tile[unsigned(y) * 36 + x] = clear
                    ? VqeafIconGolden::BACKGROUNDS[b] : pattern(icon, side, x, y);
          Raster output = {side, false};
          const uint32_t t0 = micros();
          const bool ok = VqeafIcons::decodeSpansForTest(
              static_cast<VqeafIcons::Id>(icon), side, rasterize, &output);
          decodeUs += micros() - t0;
          const uint32_t actual = crcTile(side);
          const uint32_t expected = VqeafIconGolden::CRC[index][b][clear];
          if (!ok || output.outOfBounds || actual != expected) {
            ++errors;
            ++localErrors;
            Serial.printf("[ICONTEST] FAIL %s size=%u bg=%u clear=%u "
                          "crc=%08lX expected=%08lX ok=%u bounds=%u\n",
                VqeafIcons::name(static_cast<VqeafIcons::Id>(icon)),side,b,clear,
                (unsigned long)actual,(unsigned long)expected,ok,output.outOfBounds);
          } else {
            ++passed;
          }
        }
      }
      Serial.printf("[ICONTEST] VARIANT %s size=%u cases=%u/8\n",
             VqeafIcons::name(static_cast<VqeafIcons::Id>(icon)),side,8-localErrors);
      // Timed real SPI drawing on the actual TFT, NOT a display readback test.
      // 24 variants * 3 repeats, normal splash will repaint after test.
      for (unsigned repeat = 0; repeat < 3; ++repeat) {
        const uint32_t t0 = micros();
        if (!VqeafIcons::draw(display, static_cast<VqeafIcons::Id>(icon), 0, 0,
                     side, VqeafIconGolden::BACKGROUNDS[repeat & 3], palette, true)) {
          ++errors;
          Serial.println("[ICONTEST] FAIL TFT.draw returned false");
          break;
        }
        drawUs += micros() - t0;
      }
    }
  }
#ifdef ARDUINO_ARCH_ESP32
  const uint32_t internalAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t psramAfter = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
  const uint32_t internalAfter = 0, psramAfter = 0;
#endif
  Serial.printf("[ICONTEST] MEMORY internal_before=%lu internal_after=%lu "
                "psram_before=%lu psram_after=%lu\n",
       (unsigned long)internalBefore,(unsigned long)internalAfter,
       (unsigned long)psramBefore,(unsigned long)psramAfter);
  Serial.printf("[ICONTEST] TIMING decode_192_cases_us=%lu "
                "spi_draw_72_calls_us=%lu\n",
       (unsigned long)decodeUs,(unsigned long)drawUs);
  Serial.printf("[ICONTEST] SUMMARY passed=%u expected=192 errors=%u result=%s\n",
                passed,errors,(passed==192 && errors==0) ? "PASS" : "FAIL");
  return errors + (passed!=192);
}
} // namespace VqeafIconSelfTest
#endif
