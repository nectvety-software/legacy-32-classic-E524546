#include "services/SettingsStore.h"
#include <cassert>
#include <iostream>
int main(){
  SettingsStore s; s.begin();
  assert(s.data().theme==ThemeId::S60Green);
  s.selectThemeFile("/System/Themes/s60_green.vqeaf");
  assert(s.data().theme==ThemeId::External);
  assert(s.selectedThemePath()=="/System/Themes/s60_green.vqeaf");
  SettingsStore afterReboot;afterReboot.begin();
  assert(afterReboot.data().theme==ThemeId::External);
  assert(afterReboot.selectedThemePath()=="/System/Themes/s60_green.vqeaf");
  afterReboot.selectBuiltInTheme(ThemeId::S60Green);
  SettingsStore reset;reset.begin();
  assert(reset.data().theme==ThemeId::S60Green);
  assert(!reset.selectedThemePath().length());
  std::cout<<"PERSISTENCE|external_theme_survives_reboot|built_in_restored\n";
}
