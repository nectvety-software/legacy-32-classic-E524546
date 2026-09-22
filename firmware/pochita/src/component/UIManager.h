#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "Config.h"
#include "Display.h"

extern TFT_eSPI tft;

class UIManager {
public:
  void init();
  void drawSidebar(bool wifiEnabled, bool wifiConnected);
  void drawBattery(int x, int y, int percentage, float voltage);
  void updateDock(int selectedApp);
  void drawAppIcon(int id, int x, int y, uint16_t color);
  void drawFrameContent(String title, bool isList);
  void drawList(const std::vector<String> &items, int listIndex,
                int scrollOffset, int maxVisible = 6);
};

extern UIManager uiManager;

#endif
