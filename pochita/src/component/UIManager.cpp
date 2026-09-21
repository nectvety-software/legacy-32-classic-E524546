#include "UIManager.h"

UIManager uiManager;

void UIManager::init() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BG_COLOR);
}

void UIManager::drawBattery(int x, int y, int percentage, float voltage) {
  tft.drawRoundRect(x, y, 22, 40, 3, THEME_COLOR);
  tft.fillRect(x + 6, y - 4, 10, 5, THEME_COLOR);
  int barHeight = map(percentage, 0, 100, 0, 34);
  tft.fillRect(x + 3, y + 3, 16, 34, BG_COLOR); // Clear old
  tft.fillRect(x + 3, y + 3 + (34 - barHeight), 16, barHeight,
               (percentage > 20) ? TFT_GREEN : TFT_RED);
  tft.fillRect(x - 5, y + 45, 45, 15, BG_COLOR); // Clear text
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(x - 2, y + 48);
  tft.print(voltage, 1);
  tft.print("V");
}

void UIManager::drawSidebar(bool wifiEnabled, bool wifiConnected) {
  tft.drawRect(2, 5, 55, 310, THEME_COLOR);
  tft.setTextColor(THEME_COLOR);
  tft.drawCentreString("S3-OS", 29, 10, 1);
  uint16_t wCol =
      wifiConnected ? TFT_CYAN : (wifiEnabled ? TFT_YELLOW : 0x4208);
  tft.fillCircle(29, 40, 6, wCol);
  drawBattery(18, 80, 85, 3.8);
}

void UIManager::drawAppIcon(int id, int x, int y, uint16_t color) {
  tft.fillRect(x + 4, y + 4, 40, 42, BG_COLOR);
  switch (id) {
  case 0: // WiFi
    for (int i = 0; i < 2; i++) {
      // Draw thicker arcs/circles
      tft.drawCircle(x + 24, y + 22, 6 + i, color);
      tft.drawCircle(x + 24, y + 22, 5 + i, color); // Bold

      tft.drawCircle(x + 24, y + 22, 11 + i, color);
      tft.drawCircle(x + 24, y + 22, 10 + i, color); // Bold
    }
    break;
  case 1: // SD Card
    tft.drawRect(x + 16, y + 10, 16, 24, color);
    tft.drawRect(x + 17, y + 11, 14, 22, color); // Inner bold
    break;
  case 2: // Terminal
    tft.drawRect(x + 10, y + 14, 28, 18, color);
    tft.drawRect(x + 11, y + 15, 26, 16, color); // Inner bold
    tft.drawString(">_", x + 15, y + 18, 2);
    // Bold text manually by offset
    tft.drawString(">_", x + 16, y + 18, 2);
    break;
  case 3:                                        // Apps (Grid)
    tft.fillRect(x + 13, y + 11, 10, 10, color); // Bigger blocks
    tft.fillRect(x + 25, y + 11, 10, 10, color);
    tft.fillRect(x + 13, y + 23, 10, 10, color); // Add row 2
    tft.fillRect(x + 25, y + 23, 10, 10, color);
    break;
  case 4: // Settings
    tft.drawCircle(x + 24, y + 22, 10, color);
    tft.drawCircle(x + 24, y + 22, 9, color); // Bold
    tft.drawCircle(x + 24, y + 22, 4, color); // Inner
    tft.drawCircle(x + 24, y + 22, 3, color); // Inner Bold
    break;
  }
}

void UIManager::updateDock(int selectedApp) {
  for (int i = 0; i < 5; i++) {
    int x = 10 + (i * 45);
    uint16_t col = (selectedApp == i) ? THEME_COLOR : 0x4208;
    tft.drawRoundRect(x, 285, 40, 30, 3, col);
    drawAppIcon(i, x, 285, col);
    tft.drawRoundRect(x - 1, 284, 42, 32, 3,
                      (selectedApp == i) ? TFT_WHITE : BG_COLOR);
  }
}

void UIManager::drawFrameContent(String title, bool isList) {
  tft.drawRoundRect(62, 5, 175, 270, 4, THEME_COLOR);
  tft.fillRect(75, 0, 90, 20, THEME_COLOR);
  tft.setTextColor(BG_COLOR);
  tft.drawCentreString(title, 120, 2, 2);
  tft.fillRect(67, 25, 165, 245, BG_COLOR);
}

void UIManager::drawList(const std::vector<String> &items, int listIndex,
                         int scrollOffset, int maxVisible) {
  tft.fillRect(67, 25, 165, 245, BG_COLOR);
  for (int i = 0; i < maxVisible; i++) {
    int idx = i + scrollOffset;
    if (idx >= items.size())
      break;
    int yPos = 35 + (i * 20);
    if (idx == listIndex) {
      tft.fillRect(67, yPos - 2, 160, 18, 0x2104);
      tft.setTextColor(TFT_YELLOW);
    } else
      tft.setTextColor(TFT_WHITE);
    tft.drawString(items[idx], 77, yPos, 2);
  }
}
