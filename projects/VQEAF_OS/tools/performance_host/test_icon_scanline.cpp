#include "VqeafIconRenderer.h"
#include <TFT_eSPI.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cassert>

int main() {
  const auto pal = VqeafIcons::Palette::standard();
  const uint16_t backgrounds[] = {0x0000,0xFFFF,0xAEE9,0xF81F,0x21A5,0x1234};
  unsigned long pixels=0;
  uint64_t oldSpans=0, newRows=0;
  unsigned scenarios=0;
  for (unsigned size:{24u,36u}) {
    for(unsigned icon=0;icon<12;++icon) {
      for(auto bg:backgrounds) {
        TFT_eSPI original, optimized;
        for(int yy=0;yy<int(size);++yy) for(int xx=0;xx<int(size);++xx) {
          const auto c=((xx+yy)&1)?uint16_t(0x19E6):uint16_t(0xF85F);
          original.fillRect(48+xx,59+yy,1,1,c);
          optimized.fillRect(48+xx,59+yy,1,1,c);
        }
        assert(VqeafIcons::draw(original, static_cast<VqeafIcons::Id>(icon),48,59,size,bg,pal,true));
        assert(VqeafIcons::drawOpaque(optimized, static_cast<VqeafIcons::Id>(icon),48,59,size,bg));
        assert(original.fb==optimized.fb);
        assert(optimized.imageCalls==size);
        assert(optimized.imagePixels==size*size);
        assert(optimized.beginWrites==1 && optimized.endWrites==1);
        assert(optimized.swapped==false); // getSwapBytes restored
        original.setSwapBytes(true); optimized.setSwapBytes(true);
        // Prove a different pre-existing display byte-order setting is kept.
        assert(VqeafIcons::drawOpaque(optimized,static_cast<VqeafIcons::Id>(icon),48,59,size,bg));
        assert(optimized.swapped==true);
        oldSpans += original.hlineCalls;
        newRows += size;
        pixels += size*size;
        ++scenarios;
      }
    }
  }
  TFT_eSPI rejected;
  assert(!VqeafIcons::drawOpaque(rejected,VqeafIcons::Id::Count,0,0,24,0));
  assert(!VqeafIcons::drawOpaque(rejected,VqeafIcons::Id::WiFi,0,0,25,0));
  assert(rejected.imageCalls==0 && rejected.beginWrites==0);
  std::printf("PASS RGB565 screen parity: %u variants/background scenarios, %lu pixels\n",scenarios,pixels);
  std::printf("Legacy draw calls: %llu colored H-lines; opaque fast path: %llu image scanlines, one SPI transaction per icon\n",
              (unsigned long long)oldSpans,(unsigned long long)newRows);
  std::puts("NOTE: simulated I/O call count, not measured device FPS, SPI timing or power.");
}
