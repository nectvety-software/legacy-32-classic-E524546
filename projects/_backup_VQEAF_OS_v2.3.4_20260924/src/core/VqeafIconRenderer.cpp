#include "VqeafIconRenderer.h"
#include "VqeafIconData.h"
#include <TFT_eSPI.h>
#include <string.h>
namespace VqeafIcons {
Palette Palette::standard() {
  return {{
    0x0000, // 0 transparent, ignored
    0x2187, // 1 ink      #23313A
    0xFFFF, // 2 white
    0x065D, // 3 cyan     #02CBED
    0x22FB, // 4 blue     #235FD9
    0xF952, // 5 pink     #FE2996
    0xF748, // 6 yellow   #F5EB42
    0xF524, // 7 orange   #F7A620
    0x166C, // 8 green    #16CD60
    0xD71C, // 9 silver   #D3E0E7
    0x532E, // 10 shadow  #566675
    0x9771, // 11 mint    #93ED8F
    0xF2A9, // 12 red     #F5544C
    0x6DFD  // 13 sky     #69BDEB
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
  static const char *const names[] = {"WiFi","Bluetooth","Music","Files","Gallery",
         "Internet","Shell","Recovery","Settings","Themes","Apps","Library"};
  uint8_t i=static_cast<uint8_t>(id);
  return i<12 ? names[i] : "Unknown";
}
bool draw(TFT_eSPI &tft, Id id, int16_t x, int16_t y, uint8_t size,
          uint16_t background, const Palette &palette, bool clearBackground) {
  const uint8_t i=static_cast<uint8_t>(id);
  if (i>=12 || (size!=24 && size!=36)) return false;
  const VqeafIconData::Asset &icon = size==24 ? VqeafIconData::ICONS_24[i]
                                              : VqeafIconData::ICONS_36[i];
  if (clearBackground) tft.fillRect(x,y,size,size,background);
  // Artwork is baked as small per-asset RGB565 palettes; the existing
  // Palette argument remains for ABI compatibility with legacy call sites.
  (void)palette;
  if (!icon.palette_len || icon.palette_len > 32) return false;
  uint16_t cursor=0;
  for (uint8_t row=0;row<size;++row) {
    uint8_t col=0;
    while (col<size) {
      // Strictly bounded: corrupt RLE can never read past the compiled array.
      if (cursor>=icon.bytes) return false;
      const uint8_t v=icon.rle[cursor++];
      const uint8_t role=v>>3;
      const uint8_t count=(v&7)+1;
      if (role>=icon.palette_len || count>size-col) return false;
      if (role) tft.drawFastHLine(x+col,y+row,count,icon.rgb565[role]);
      col+=count;
    }
  }
  return cursor==icon.bytes;
}
void focusFrame(TFT_eSPI &tft, int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t outer, uint16_t inner) {
  if (w<6 || h<6) return;
  tft.drawRect(x,y,w,h,outer);
  tft.drawRect(x+1,y+1,w-2,h-2,inner);
}
} // namespace VqeafIcons
