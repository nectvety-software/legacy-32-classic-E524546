// Generate reference checksums from the FROZEN v2.3.4 renderer and RGB565 data.
// Never compile this utility or the baseline arrays into optimized production.
#include <TFT_eSPI.h>
#include "VqeafIconRenderer.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>

static uint16_t pattern(unsigned icon, unsigned side, unsigned x, unsigned y) {
  return uint16_t((icon << 8) ^ (x * 23) ^ (y * 41) ^ ((x * y) << 2) ^ (side << 7));
}
static uint32_t crc32Pixel(uint32_t crc, uint16_t pix) {
  for (int b = 0; b < 2; ++b) {
    crc ^= (b ? (pix >> 8) : (pix & 255));
    for (int k = 0; k < 8; ++k)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
  }
  return crc;
}
int main() {
  constexpr uint16_t BACKGROUND[4] = {0x0000,0xffff,0x7bef,0xf81f};
  TFT_eSPI t;
  printf("[\n");
  for (unsigned icon = 0; icon < 12; ++icon) {
    for (unsigned side : {24u, 36u}) {
      printf("  [");
      for (unsigned bi = 0; bi < 4; ++bi) {
        printf("[");
        for (unsigned clear = 0; clear < 2; ++clear) {
          for (unsigned y = 0; y < side; ++y)
            for (unsigned x = 0; x < side; ++x)
              t.fb[y*TFT_eSPI::W+x] = pattern(icon, side, x, y);
          if (!VqeafIcons::draw(t, static_cast<VqeafIcons::Id>(icon), 0, 0,
                                uint8_t(side), BACKGROUND[bi],
                                VqeafIcons::Palette::standard(), clear != 0)) return 3;
          uint32_t crc = 0xffffffffu;
          for (unsigned y = 0; y < side; ++y)
            for (unsigned x = 0; x < side; ++x)
              crc = crc32Pixel(crc, t.fb[y*TFT_eSPI::W+x]);
          printf("%s%u", clear ? "," : "", ~crc);
        }
        printf("]%s", bi==3 ? "" : ",");
      }
      printf("]%s\n", (icon==11 && side==36) ? "" : ",");
    }
  }
  printf("]\n");
  return 0;
}
