#include <cassert>
#include <cstring>
#include <iostream>
#include "core/UiLayoutGeometry.h"
#include "core/UiScreenAtlas.h"
int main() {
  using namespace VqeafLayout;
  static_assert(W==240 && H==320 && CONTENT_TOP==28 && FOOTER_Y==298, "portrait contract");
  static_assert(gridCell(0,0).x==1 && gridCell(2,3).x==157, "grid x contract");
  static_assert(gridCell(2,3).y==226 && gridCell(2,3).bottom()==292, "grid y contract");
  static_assert(homeShortcut(0).x==8 && homeShortcut(2).x==160, "home quick tiles");
  static_assert(listRow(5).bottom()==280, "6 rows must leave footer clear");
  static_assert(softkey(2).right()==240, "softkey trio");
  assert(VqeafAtlas::screenCount==16);
  size_t count=0;
  for (size_t i=0; i<VqeafAtlas::screenCount; ++i) {
    const auto &screen=VqeafAtlas::screens[i];
    assert(screen.id&&screen.name&&screen.count>0);
    assert(VqeafAtlas::get(screen.id)==&screen);
    for (size_t j=0;j<screen.count;++j) {
      const auto &region=screen.regions[j];
      if (!inScreen(region.rect)) {
        std::cerr<<"OUT OF BOUNDS: "<<screen.id<<"/"<<region.name<<"\n";
        return 1;
      }
      // Screen-specific content never crosses y=298. Shared footer separate.
      if (region.rect.y>=CONTENT_TOP && region.rect.bottom()>FOOTER_Y) {
        std::cerr<<"OVER FOOTER: "<<screen.id<<"/"<<region.name<<"\n";
        return 1;
      }
      ++count;
    }
  }
  const auto *menu=VqeafAtlas::get("menu"); assert(menu->count==37); // 12 cells+icons+captions + rail
  const auto *home=VqeafAtlas::get("home"); assert(home->count==10);
  assert(VqeafAtlas::get("not-a-screen")==nullptr);
  std::cout<<"PASS: "<<VqeafAtlas::screenCount<<" screens, "<<count
           <<" target regions, all within 240x320. Grid 3x4 and 3 softkeys safe.\n";
  return 0;
}
