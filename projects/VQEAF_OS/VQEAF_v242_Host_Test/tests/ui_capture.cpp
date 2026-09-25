// Production SymbianUI.cpp pixel renderer with host reference TFT 240x320 shim.
// UI accepts theme RGB565 colors produced by real ThemeFileService host capture.
#include "core/SymbianUI.h"
#include "core/UiIconCatalog.h"
#include <cassert>
#include <cstdlib>
#include <string>
#include <cstdio>
static void save(TFT_eSPI &lcd,const std::string& root,const char* name){
  lcd.savePPM((root+"/"+name+".ppm").c_str());
}
int main(int argc, char **argv){
 assert(argc==16); // program out path + 13 theme colors + 1 screen style flag
 std::string root=argv[1];
 ThemeColors palette=themeFor(ThemeId::S60Green);
 uint16_t *ptr=&palette.bg;
 for(int i=0;i<13;i++)ptr[i]=uint16_t(strtoul(argv[i+2],nullptr,10));
 TFT_eSPI screen; SymbianUI ui(screen);
 ui.setTheme(ThemeId::S60Green);
 ui.clear();ui.chrome("App Installer",false,false,true,false);
 ui.message("Verified app","Welcome v1.0.0","Signature: verified","Press OK to install");
 ui.softkeys("Details","Install","Back");
 save(screen,root,"01_installer_ready");
 ui.clear();ui.chrome("App Manager",false,false,true,false);
 ui.listItem(0,"Apps","Welcome","Installed | signed",true);
 ui.listItem(1,"Apps","Help Site","Installed | signed",false);
 ui.softkeys("Options","Open","Back");
 save(screen,root,"02_apps_installed");
 LauncherStyle skin=LauncherStyle::fromPalette(palette);
 ui.setExternalTheme(palette,&skin);
 ui.clear();ui.chrome("Themes",false,false,true,false);
 ui.listItem(0,"Themes","S60 Green","Applied from microSD",true);
 ui.softkeys("Options","Apply","Back");
 save(screen,root,"03_theme_applied");
 ui.clear();ui.chrome("Menu",false,false,true,false);ui.menuBackground();
 const char* titles[]={"WiFi","Bluetooth","Music","File mgr","Gallery","Internet","Shell","Recovery","Settings","Themes","Apps","Library"};
 for(int i=0;i<12;i++)ui.gridItem(i,UiIconCatalog::GRID_IDS[i],titles[i],i==9);
 ui.softkeys("Options","Open","Exit");
 save(screen,root,"04_menu_new_theme");
 ui.setTheme(ThemeId::S60Green);
 ui.clear();ui.chrome("Internet",false,false,false,false);
 ui.listItem(0,"Internet","VQEAF Browser Test","HTTP 200 | host HTTP",true);
 ui.listItem(1,"Internet","Qeafbrowser","HTML content parsed",false);
 ui.listItem(2,"Internet","App downloads","1 hyperlink found",false);
 ui.softkeys("Options","Open","Back");
 save(screen,root,"05_browser_page");
 printf("HOST_CPP_240x320_RGB565_SNAPSHOTS|5\n");
}
