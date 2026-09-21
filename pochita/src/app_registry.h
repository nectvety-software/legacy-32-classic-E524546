#ifndef APP_REGISTRY_H
#define APP_REGISTRY_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "asset/bitmap_64dp.h"
#include "asset/bitmap_app_icons.h"

enum AppType {
  APP_SYSTEM,
  APP_INSTALLED
};

struct AppItem {
  String name;
  const uint32_t* icon;
  AppType type;
  String filePath;
  int systemId;
  int iconWidth;
  int iconHeight;
};

std::vector<AppItem> appList;

void saveInstalledApps() {
  String data;
  for (const auto &app : appList) {
    if (app.type != APP_INSTALLED) continue;
    data += app.name + "|" + app.filePath + "\n";
  }
  Preferences prefs;
  prefs.begin("launcher", false);
  prefs.putString("installed", data);
  prefs.end();
}

void appendInstalledApp(const String &name, const String &path) {
  if (!path.length()) return;
  for (const auto &app : appList)
    if (app.filePath == path) return;
  appList.push_back({name, SDCARD, APP_INSTALLED, path, -1,
                     SDCARD_WIDTH, SDCARD_HEIGHT});
}

void loadInstalledApps() {
  Preferences prefs;
  prefs.begin("launcher", true);
  String data = prefs.getString("installed", "");
  prefs.end();
  int start = 0;
  while (start < data.length()) {
    int end = data.indexOf('\n', start);
    if (end < 0) end = data.length();
    String row = data.substring(start, end);
    int separator = row.indexOf('|');
    if (separator > 0)
      appendInstalledApp(row.substring(0, separator),
                         row.substring(separator + 1));
    start = end + 1;
  }
}

void initApps() {
  appList.clear();
  appList.push_back({"WiFi",    ic_wifi,      APP_SYSTEM, "", 0,  IC_WIFI_W,      IC_WIFI_H});
  appList.push_back({"Files",   ic_files,     APP_SYSTEM, "", 1,  IC_FILES_W,     IC_FILES_H});
  appList.push_back({"Bluetooth", ic_ble,     APP_SYSTEM, "", 2,  IC_BLE_W,       IC_BLE_H});
  appList.push_back({"Terminal", ic_terminal, APP_SYSTEM, "", 3,  IC_TERMINAL_W,  IC_TERMINAL_H});
  appList.push_back({"Notes",   ic_script,    APP_SYSTEM, "", 4,  IC_SCRIPT_W,    IC_SCRIPT_H});
  appList.push_back({"LoRa",    ic_satellite, APP_SYSTEM, "", 5,  IC_SATELLITE_W, IC_SATELLITE_H});
  appList.push_back({"IR",      ic_sniffer,   APP_SYSTEM, "", 6,  IC_SNIFFER_W,   IC_SNIFFER_H});
  appList.push_back({"Browser", ic_web,       APP_SYSTEM, "", 7,  IC_WEB_W,       IC_WEB_H});
  appList.push_back({"Settings", ic_settings, APP_SYSTEM, "", 8,  IC_SETTINGS_W,  IC_SETTINGS_H});
  appList.push_back({"Radio",   ic_radio,     APP_SYSTEM, "", 9,  IC_RADIO_W,     IC_RADIO_H});
  appList.push_back({"Music",   ic_audio,     APP_SYSTEM, "", 10, IC_AUDIO_W,     IC_AUDIO_H});
  appList.push_back({"Paint",   ic_paint,     APP_SYSTEM, "", 11, IC_PAINT_W,     IC_PAINT_H});
  appList.push_back({"Retro",   ic_retro,     APP_SYSTEM, "", 12, IC_RETRO_W,     IC_RETRO_H});
  appList.push_back({"Chat",    ic_robot,     APP_SYSTEM, "", 13, IC_ROBOT_W,     IC_ROBOT_H});
  appList.push_back({"VM",      ic_explorer,  APP_SYSTEM, "", 14, IC_EXPLORER_W,  IC_EXPLORER_H});
  loadInstalledApps();
}

void installApp(String name, String path) {
  appendInstalledApp(name, path);
  saveInstalledApps();
}

void removeApp(String path) {
  for (size_t i = 0; i < appList.size(); i++) {
    if (appList[i].filePath == path) {
      appList.erase(appList.begin() + i);
      saveInstalledApps();
      return;
    }
  }
}

void uninstallApp(int index) {
  if (index >= 0 && index < appList.size() && appList[index].type == APP_INSTALLED) {
    appList.erase(appList.begin() + index);
    saveInstalledApps();
  }
}

void clearLauncher() {
  for (int i = appList.size() - 1; i >= 0; i--) {
    if (appList[i].type == APP_INSTALLED) {
      appList.erase(appList.begin() + i);
    }
  }
  saveInstalledApps();
}

#endif
