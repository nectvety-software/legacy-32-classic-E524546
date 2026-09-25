// Compile this file against real src/core/SymbianUI.cpp and the framebuffer TFT
// host shim. This test never substitutes the production icon rasterizer/UI.
#include "core/SymbianUI.h"
#include "core/UiIconCatalog.h"
#include "core/UiLayoutGeometry.h"
#include "core/VqeafIconRenderer.h"
#include <cassert>
#include <cstdio>
#include <string>

using VqeafIcons::Id;

static uint16_t menuBand(int row) {
  static const uint16_t bands[] = {0x75A3, 0x8E25, 0x96A6, 0x8E44};
  assert(row>=0 && row<4);
  return bands[row];
}

static void pixelParity(const TFT_eSPI &rendered, Id icon, int x, int y,
                        int size, uint16_t bg) {
  assert(size==24 || size==36);
  TFT_eSPI canonical;
  const bool ok = VqeafIcons::draw(canonical,icon,x,y,(uint8_t)size,bg,
                                   VqeafIcons::Palette::standard());
  assert(ok);
  int opaque = 0;
  for(int j=0;j<size;++j)for(int i=0;i<size;++i) {
    uint16_t actual=rendered.get(x+i,y+j);
    uint16_t expect=canonical.get(x+i,y+j);
    if(actual != expect) {
      std::fprintf(stderr,"Glyph mismatch %s size=%d pixel=(%d,%d) expected=0x%04x actual=0x%04x\n",
                   VqeafIcons::name(icon),size,x+i,y+j,expect,actual);
      assert(false);
    }
    if (actual!=bg) ++opaque;
  }
  assert(opaque > 20);
}

int main(int argc, char **argv) {
  assert(argc>=2);
  const std::string path=argv[1];
  const ThemeColors lime=themeFor(ThemeId::S60Green);
  TFT_eSPI lcd;
  SymbianUI ui(lcd);
  ui.setTheme(ThemeId::S60Green);

  // 3 Home shortcuts must be EXACTLY the same bytes as Menu's WiFi/Music/Files.
  assert(UiIconCatalog::homeAsset(0)==UiIconCatalog::menuAsset(0));
  assert(UiIconCatalog::homeAsset(1)==UiIconCatalog::menuAsset(2));
  assert(UiIconCatalog::homeAsset(2)==UiIconCatalog::menuAsset(3));
  assert(UiIconCatalog::homeAsset(3)==Id::Count);
  ui.idleHome(false,false,false,false,0,4,false,"WiFi: no saved networks");
  for(int i=0;i<3;++i) {
    const VqeafLayout::Rect r=VqeafLayout::homeShortcut(i);
    assert(VqeafLayout::inScreen(r));
    pixelParity(lcd,UiIconCatalog::homeAsset(i),r.x+(r.w-36)/2,r.y+6,
                36, i==0 ? lime.selected : lime.panel);
  }
  lcd.savePPM((path+"home.ppm").c_str());
  auto previous=lcd.fb;
  int fullBefore=lcd.fullFills;
  ui.idleShortcutDelta(0,1);
  assert(fullBefore==lcd.fullFills);
  for(int y=0;y<320;++y)for(int x=0;x<240;++x) {
    if (previous[y*240+x]!=lcd.fb[y*240+x]) {
      const VqeafLayout::Rect a=VqeafLayout::homeShortcut(0);
      const VqeafLayout::Rect b=VqeafLayout::homeShortcut(1);
      const bool insideA=x>=a.x && x<a.right() && y>=a.y && y<a.bottom();
      const bool insideB=x>=b.x && x<b.right() && y>=b.y && y<b.bottom();
      assert(insideA || insideB);
    }
  }
  const VqeafLayout::Rect r0=VqeafLayout::homeShortcut(0),r1=VqeafLayout::homeShortcut(1);
  pixelParity(lcd,UiIconCatalog::homeAsset(0),r0.x+(r0.w-36)/2,r0.y+6,36,lime.panel);
  pixelParity(lcd,UiIconCatalog::homeAsset(1),r1.x+(r1.w-36)/2,r1.y+6,36,lime.selected);
  lcd.savePPM((path+"home_selected.ppm").c_str());

  ui.clear();
  ui.chrome("Menu",false,false,false,false);
  ui.menuBackground();
  static const char *const title[]={"WiFi","Bluetooth","Music","File mgr",
       "Gallery","Internet","Shell","Recovery","Settings","Themes","Apps","Library"};
  for(int i=0;i<12;++i) {
    assert(UiIconCatalog::menuAsset(i)!=Id::Count);
    assert(UiIconCatalog::menuAsset(i)==VqeafIcons::fromLegacy(
        UiIconCatalog::canonical(UiIconCatalog::GRID_IDS[i])));
    ui.gridItem(i,UiIconCatalog::GRID_IDS[i],title[i],i==0);
  }
  ui.softkeys("Options","Open","Exit");
  for(int i=0;i<12;++i) {
    const VqeafLayout::Rect r=VqeafLayout::gridIcon(i%3,i/3);
    assert(VqeafLayout::inScreen(r));
    pixelParity(lcd,UiIconCatalog::menuAsset(i),r.x,r.y,36,
                i==0 ? lime.selected : menuBand(i/3));
  }
  assert(UiIconCatalog::menuAsset(-1)==Id::Count);
  assert(UiIconCatalog::menuAsset(12)==Id::Count);
  lcd.savePPM((path+"menu.ppm").c_str());
  previous=lcd.fb;fullBefore=lcd.fullFills;
  ui.gridItem(0,UiIconCatalog::GRID_IDS[0],title[0],false);
  ui.gridItem(4,UiIconCatalog::GRID_IDS[4],title[4],true);
  assert(fullBefore==lcd.fullFills);
  for(int y=0;y<320;++y)for(int x=0;x<240;++x) {
    if(previous[y*240+x]==lcd.fb[y*240+x])continue;
    const VqeafLayout::Rect a=VqeafLayout::gridCell(0,0);
    const VqeafLayout::Rect b=VqeafLayout::gridCell(1,1);
    const bool inA=x>=a.x && x<a.right() && y>=a.y && y<a.bottom();
    const bool inB=x>=b.x && x<b.right() && y>=b.y && y<b.bottom();
    assert(inA || inB);
  }
  for(int i=0;i<12;++i) {
    const VqeafLayout::Rect r=VqeafLayout::gridIcon(i%3,i/3);
    pixelParity(lcd,UiIconCatalog::menuAsset(i),r.x,r.y,36,
                i==4 ? lime.selected : menuBand(i/3));
  }
  lcd.savePPM((path+"menu_selected.ppm").c_str());

  // A list row uses a separately rasterized 24x24 variant of the same identity.
  // This confirms none of the 36px assets accidentally appear in compact rows.
  for (int i=0;i<12;++i) {
    ui.clear();
    ui.listItem(0,UiIconCatalog::GRID_IDS[i],title[i],"System",false);
    pixelParity(lcd,UiIconCatalog::menuAsset(i),12,
                VqeafLayout::CONTENT_TOP+1+9,24,lime.bg);
  }
  // Theme focus frame remains driven by colors, not hardcoded into bitmap.
  ui.setTheme(ThemeId::AmoledRed);
  ui.clear();ui.chrome("Menu",false,false,false,false);ui.menuBackground();
  ui.gridItem(0,"Wi",title[0],true);
  assert(lcd.get(2,29)==themeFor(ThemeId::AmoledRed).accent);
  pixelParity(lcd,Id::WiFi, VqeafLayout::gridIcon(0,0).x,
              VqeafLayout::gridIcon(0,0).y,36,
              themeFor(ThemeId::AmoledRed).selected);
  // A custom launcher skin produced by an imported .vqeaf file also owns
  // focus colors, while the standardized icon identity and artwork stay intact.
  LauncherStyle skin=LauncherStyle::fromPalette(lime);
  skin.tabAccent=0x07E0;
  skin.border=0xF800;
  skin.selectedBg=0x1234;
  skin.selectedFg=0xFFFF;
  ui.setExternalTheme(lime,&skin);
  ui.clear();ui.chrome("Menu",false,false,false,false);ui.menuBackground();
  ui.gridItem(0,"Wi",title[0],true);
  assert(lcd.get(2,29)==skin.tabAccent); // outer focus line
  assert(lcd.get(3,30)==skin.border);    // inner focus line
  pixelParity(lcd,Id::WiFi,VqeafLayout::gridIcon(0,0).x,
              VqeafLayout::gridIcon(0,0).y,36,skin.selectedBg);
  std::puts("PASS VQEAF v2.3.2: 12 Menu + 3 Home (36px), all 12 lists (24px), bounded focus redraw, built-in/custom theme focus");
}
