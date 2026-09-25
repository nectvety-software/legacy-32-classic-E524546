// Standalone, buildable against BOTH 2.3.4 reference and 2.3.5 optimized
// C++ icon renderers. Byte-for-byte comparison catches palette or crop changes.
#include "VqeafIconRenderer.h"
#include <TFT_eSPI.h>
#include <fstream>
#include <cstdint>
#include <cstdio>

int main(int argc,char **argv) {
  if(argc!=2) return 2;
  std::ofstream out(argv[1],std::ios::binary);
  if(!out) return 3;
  TFT_eSPI lcd;
  const uint16_t colors[]={0x0000,0xAEE9,0xFFFF,0xF81F};
  const auto palette=VqeafIcons::Palette::standard();
  for(uint8_t size : {uint8_t(24),uint8_t(36)}) {
    for(uint8_t id=0;id<12;++id) {
      for(uint16_t color:colors) {
        for(uint8_t clear=0;clear<2;++clear) {
          // Checker prefill: verifies both transparency and background clearing.
          for(int y=0;y<size;++y) for(int x=0;x<size;++x)
            lcd.fillRect(10+x,10+y,1,1,((x+y)&1) ? 0x8010 : 0x1234);
          if(!VqeafIcons::draw(lcd,static_cast<VqeafIcons::Id>(id),10,10,size,
                               color,palette,clear!=0)) return 4;
          for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
            const uint16_t pixel=lcd.fb[(10+y)*240+(10+x)];
            out.put(char(pixel&255));out.put(char(pixel>>8));
          }
        }
      }
    }
  }
  if(!out) return 5;
  std::puts("PASS: wrote all C++ 24/36 RGB565 screen scenarios");
}
