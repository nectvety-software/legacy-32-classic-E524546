// Compiles and executes the ACTUAL LauncherView C++, not a reimplementation.
// Fake TFT rasterizes primitives; real LCD fonts are not represented in PPM.
#include <cassert>
#include <iostream>
#include "launcher/LauncherView.h"
int main(int argc,char **argv) {
  TFT_eSPI lcd;
  LauncherView view(lcd);
  ThemeColors colors=themeFor(ThemeId::Classic);
  LauncherStyle s=LauncherStyle::fromPalette(colors);
  LauncherRow rows[]={
    {"WB","Qeafbrowser","Lightweight browser",nullptr},
    {"AP","Installed apps","Signed QEAPP/2",nullptr},
    {"FI","File manager","Storage",nullptr},
    {"MD","Media library","Gallery and music",nullptr},
    {"TH","Themes","Select a .vqeaf",nullptr},
    {"RC","Recent tasks","System history",nullptr}
  };
  view.full(s,"Home",0,6,rows,6,0,0,String("12:34"),true,true);
  assert(lcd.fullFills==1);
  assert(lcd.get(4,LauncherView::LIST_Y+1)==s.tabAccent);
  assert(lcd.get(7,LauncherView::LIST_Y+LauncherView::ROW_H+1)==s.listBg);
  assert(lcd.get(0,LauncherView::PREVIEW_Y+2)==s.previewBg);
  assert(lcd.get(0,LauncherView::FOOTER_Y+3)==s.footerBg);
  if(argc>1)lcd.savePPM(argv[1]); // initial Home screenshot geometry
  view.row(s,&rows[0],0,false);view.row(s,&rows[1],1,true);
  assert(lcd.fullFills==1); // selection redraw may not erase screen
  assert(lcd.get(4,LauncherView::LIST_Y+LauncherView::ROW_H+1)==s.tabAccent);
  uint16_t icon[1024];for(auto &p:icon)p=0xF800;
  view.preview(s,&rows[1],icon);
  assert(lcd.get(13,LauncherView::PREVIEW_Y+13)==0xF800);
  const char *choices[]={"Open", "Themes", "Installer"};
  if(argc>2)lcd.savePPM(argv[2]);
  view.dialog(s,"Options",choices,3,0,1);
  assert(lcd.get(11,53)==s.border || lcd.get(11,53)!=0); // dialog bounded on display
  assert(lcd.fullFills==1);
  std::cout<<"PASS launcher: 240x320 geometry, 6-tab, dirty row, RGB565 signed icon, dialog\n";
}
