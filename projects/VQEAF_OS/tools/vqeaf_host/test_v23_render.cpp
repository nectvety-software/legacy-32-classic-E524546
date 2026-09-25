// Use ACTUAL production SymbianUI.cpp with host-only framebuffer/TFT shim.
#include "core/SymbianUI.h"
#include "core/UiIconCatalog.h"
#include "core/UiVietnameseFont.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
static bool cell(int x,int y,int c,int r){return x>=1+c*78&&x<1+c*78+76&&y>=28+r*66&&y<28+r*66+66;}
static bool tile(int x,int y,int i){int a=8+i*76;return x>=a&&x<a+72&&y>=191&&y<258;}
int main(int argc,char **argv){
 assert(argc>=2);
 TFT_eSPI disp;
 SymbianUI ui(disp);
 ui.setTheme(ThemeId::S60Green);
 ui.idleHome(false,false,false,false,0,4,false,"WiFi: no saved networks");
 assert(disp.get(0,0)==0x2B63); assert(disp.get(20,70)==0xDF93);assert(disp.get(20,150)==0xB6EE);
 // RGB565 screenshot baseline, base/selected panels are exact screenshot colors.
 assert(disp.get(90,247)==0xAEE9);assert(disp.get(17,245)==0xDF93);
 std::string base=argv[1];disp.savePPM((base+"home.ppm").c_str());
 auto before=disp.fb;int allBefore=disp.fullFills;
 ui.idleShortcutDelta(0,1);assert(disp.fullFills==allBefore);
 int diff=0;for(int y=0;y<320;++y)for(int x=0;x<240;++x)if(before[y*240+x]!=disp.fb[y*240+x]){assert(tile(x,y,0)||tile(x,y,1));++diff;}
 assert(diff>100);disp.savePPM((base+"home_selected.ppm").c_str());
 ui.clear();ui.chrome("Menu",false,false,false,false);ui.menuBackground();
 const char *titles[]={"WiFi","Bluetooth","Music","File mgr","Gallery","Internet","Shell","Recovery","Settings","Themes","Apps","Library"};
 for(int i=0;i<12;++i)ui.gridItem(i,UiIconCatalog::GRID_IDS[i],titles[i],i==0);
 ui.softkeys("Options","Open","Exit");
 assert(disp.get(12,100)==0x8E25); assert(disp.get(12,162)==0x96A6);
 assert(disp.get(230,40)==0x75A3);
 assert(disp.get(2,29)==0xDF93||disp.get(2,29)==0xFFFF);
 disp.savePPM((base+"menu.ppm").c_str());
 before=disp.fb;allBefore=disp.fullFills;
 ui.gridItem(0,UiIconCatalog::GRID_IDS[0],titles[0],false);
 ui.gridItem(4,UiIconCatalog::GRID_IDS[4],titles[4],true);
 assert(disp.fullFills==allBefore);
 diff=0;for(int y=0;y<320;++y)for(int x=0;x<240;++x)
  if(before[y*240+x]!=disp.fb[y*240+x]){assert(cell(x,y,0,0)||cell(x,y,1,1));++diff;}
 assert(diff>100);disp.savePPM((base+"menu_selected.ppm").c_str());
 assert(UiVietnameseFont::hasUtf8("Trình duyệt"));
 assert(UiVietnameseFont::measure("Tiếng Việt có dấu",1)>0);
 auto clipped=ui.fitTextPixels("Quản lý tệp tin dài",2,44);
 assert(clipped.length()>0 && ui.textWidth(clipped,2)<=44);
 // Incomplete 2/3 byte suffixes are never produced by pixel clipping.
 const char *p=clipped.c_str();while(*p) {
   uint32_t cp=UiVietnameseFont::next(p);
   assert(cp!='?' && cp!=0xfffd);
 }
 assert(UiVietnameseFont::lookup(0x1EA1,1)->cp==0x1EA1); // ạ
 for(const char *s:UiIconCatalog::GRID_IDS)assert(UiIconCatalog::known(s));
 ui.clear();ui.chrome("Font",false,false,false,false);
 ui.textBold(8,45,"Trình duyệt",2,0x0000,0xAEE9);
 ui.textBold(8,78,"Âm nhạc",2,0x0000,0xAEE9);
 ui.textBold(8,111,"Cài đặt",2,0x0000,0xAEE9);
 ui.textBold(8,144,"Tải xuống",2,0x0000,0xAEE9);
 ui.textBold(8,177,"Kết nối mạng",2,0x0000,0xAEE9);
 ui.textBold(8,215,"Đánh dấu / Yêu thích",1,0x0000,0xAEE9);
 disp.savePPM((base+"font_vi.ppm").c_str());
 puts("PASS: 240x320 Home/Menu real C++ host raster + screenshot RGB565 sample + two-cell dirty + Vietnamese UTF-8 + 12 icons");
}
