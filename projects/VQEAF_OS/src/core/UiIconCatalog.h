#pragma once
#include <string.h>
#include "VqeafIconRenderer.h"
// Identical semantic glyph IDs in Home shortcuts, Menu grid, App lists and
// Recovery/Settings. Physical Home/Menu draws standardized 36x36 flash-RLE icons; 24x24 in
// system lists. Legacy procedural fallback is reserved for extra app IDs.
namespace UiIconCatalog {
  static constexpr int BOX=36;
  static constexpr const char *GRID_IDS[12] = {
    "Wi", "BLE", "Mus", "Dir", "Pic", "Web", "Term", "Rec", "Set", "Th", "App", "Col"
  };
  // Single semantic routing table for the reference 3x4 Main Menu and Home.
  // Keep the order aligned with LauncherGrid.cpp and VqeafIcons::Id.
  // Keeping this table independent of visible labels enables full localization.
  static constexpr VqeafIcons::Id GRID_ASSETS[12] = {
    VqeafIcons::Id::WiFi, VqeafIcons::Id::Bluetooth, VqeafIcons::Id::Music,
    VqeafIcons::Id::Files, VqeafIcons::Id::Gallery, VqeafIcons::Id::Internet,
    VqeafIcons::Id::Shell, VqeafIcons::Id::Recovery, VqeafIcons::Id::Settings,
    VqeafIcons::Id::Themes, VqeafIcons::Id::Apps, VqeafIcons::Id::Library
  };
  static constexpr VqeafIcons::Id HOME_ASSETS[3] = {
    VqeafIcons::Id::WiFi, VqeafIcons::Id::Music, VqeafIcons::Id::Files
  };
  inline VqeafIcons::Id menuAsset(int position) {
    return (position >= 0 && position < 12) ? GRID_ASSETS[position]
                                           : VqeafIcons::Id::Count;
  }
  inline VqeafIcons::Id homeAsset(int position) {
    return (position >= 0 && position < 3) ? HOME_ASSETS[position]
                                          : VqeafIcons::Id::Count;
  }
  inline const char *canonical(const char *key) {
    if (!key) return "App";
    if (!strcmp(key,"BT"))return "BLE";
    if (!strcmp(key,"Sh"))return "Term";
    if (!strcmp(key,"File"))return "Doc";
    if (!strcmp(key,"All"))return "App";
    if (!strcmp(key,"Sys"))return "Set";
    return key;
  }
  inline bool known(const char *id) {
    const char *const list[]={"Wi","BLE","Mus","Dir","Pic","Web","Term","Rec", "Set","Th","App","Col", "Doc","Note","Clk","Br","i","Bell","Lock","Quick"};
    const char *key=canonical(id);
    for(const char *name:list)if(!strcmp(key,name))return true;
    return false;
  }
}
