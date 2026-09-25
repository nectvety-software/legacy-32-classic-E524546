#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Theme.h"
#include "UiLayoutGeometry.h"
#include "../launcher/LauncherTheme.h"

class SymbianUI {
public:
  explicit SymbianUI(TFT_eSPI &display) : tft(display) {}

  void setTheme(ThemeId id);
  void setExternalTheme(const ThemeColors &palette, const LauncherStyle *skin = nullptr);
  const LauncherStyle &getLauncherStyle() const { return launcherSkin; }
  ThemeId getTheme() const { return themeId; }
  ThemeColors c() const { return colors; }

  void begin();
  void clear();
  void clearContent();
  void clearListRow(int row);
  void invalidateChrome();

  void chrome(const String &title, bool wifi, bool ble, bool sd, bool hour12);
  void refreshWifiBadge(bool connected, bool hour12);
  void softkeys(const String &left, const String &center, const String &right);
  void listItem(int row, const String &icon, const String &title, const String &sub, bool selected);
  void menuBackground();
  void gridItem(int slot, const String &icon, const String &title, bool selected);
  void gridHint(const String &title, const String &sub);
  void message(const String &title, const String &line1, const String &line2 = "", const String &line3 = "");
  void dialog(const String &title, const String &line1, const String &line2,
              const String &left, const String &right, int selected = 1);
  void idleHome(bool wifi, bool ble, bool sd, bool hour12, int shortcutIndex, int unreadNotifications, bool musicPlaying, const String &wifiLine = String());
  void idleClock(bool hour12);
  void idleNetworkStatus(const String &line, bool connected); // only repaint standby status row + glyphs
  void idleShortcuts(int shortcutIndex);
  void idleShortcutDelta(int previous, int next); // only two affected Home tiles
  void lockScreen(bool wifi, bool ble, bool sd, bool hour12, int unreadNotifications, bool dimmed);
  void lockClock(bool hour12, bool dimmed);
  void clockFace(bool hour12, bool full);
  void progress(int x, int y, int w, int pct);
  void drawIcon(int x, int y, const String &kind, uint16_t color);
  void scrollbar(int total, int visible, int offset, int topY = CONTENT_TOP + 2, int bottomY = SOFTKEY_TOP - 4);
  void popupMenu(const char *const items[], int count, int selected, int offset = 0, int visible = 5);
  void transitionOut();
  void openingApp(const String &name, const String &icon, bool resume = false);
  String timeText(bool hour12);
  String dateText();
  TFT_eSPI &display() { return tft; }
  void textBold(int x, int y, const String &text, uint8_t font, uint16_t fg, uint16_t bg, bool extraBold = false);
  int textWidth(const String &text, uint8_t font = 1);
  String fitTextPixels(const String &label, uint8_t font, int maxPx);

  static constexpr int TITLEBAR_H = VqeafLayout::TITLEBAR_H;
  static constexpr int CONTENT_TOP = VqeafLayout::CONTENT_TOP;
  static constexpr int SOFTKEY_H = VqeafLayout::FOOTER_H;
  static constexpr int SOFTKEY_TOP = VqeafLayout::FOOTER_Y;
  static constexpr int LIST_ROW_H = VqeafLayout::LIST_ROW_H;
  static constexpr int LIST_VISIBLE = VqeafLayout::LIST_VISIBLE;
  static constexpr int GRID_COLS = VqeafLayout::GRID_COLS;
  static constexpr int GRID_ROWS = VqeafLayout::GRID_ROWS;
  static constexpr int GRID_CELL_W = VqeafLayout::GRID_W;
  static constexpr int GRID_CELL_H = VqeafLayout::GRID_H;
  static constexpr int ICON_BOX = VqeafLayout::ICON_BOX;

  // S60 title/status bar keeps the clock centered on the 240px LCD. The right
  // 80px zone contains only WiFi + battery; v1.1 groups those two glyphs tightly
  // at the right edge because this board has no SIM/cellular indicators.
  static constexpr int STATUS_ZONE_W = VqeafLayout::STATUS_ZONE_W;
  static constexpr int STATUS_ICON_W = VqeafLayout::STATUS_ICON_W;
  static constexpr int STATUS_ICON_GAP = VqeafLayout::STATUS_ICON_GAP;
  static constexpr int STATUS_RIGHT_PAD = VqeafLayout::STATUS_RIGHT_PAD;

private:
  TFT_eSPI &tft;
  ThemeId themeId = ThemeId::Classic;
  uint16_t selectedInk = 0xFFFF;  // launcher.selectedFg for highlighted list/grid labels
  ThemeColors colors = themeFor(ThemeId::Classic);
  LauncherStyle launcherSkin = LauncherStyle::fromPalette(themeFor(ThemeId::Classic));

  bool chromeValid = false;
  bool softkeysValid = false;
  bool chromeWifi = false;
  bool chromeBle = false;
  bool chromeSd = false;
  bool chromeHour12 = false;
  uint8_t chromeWifiBars = 0;
  char chromeTitle[24] = {0};
  char chromeClock[12] = {0};
  char softLeft[18] = {0};
  char softCenter[18] = {0};
  char softRight[18] = {0};

  void pixelLineH(int x, int y, int w, uint16_t c, int s = 2);
  void pixelLineV(int x, int y, int h, uint16_t c, int s = 2);
  uint8_t statusWifiBars() const;
  void drawStatusWifi(int x, int y, uint16_t c);
  void drawStatusBle(int x, int y, uint16_t c);
  void drawStatusSd(int x, int y, uint16_t c);
  void drawStatusBattery(int x, int y, uint16_t c);
  void drawWallpaper();
  void idleShortcutTile(int i,bool selected);
  uint16_t menuRowColor(int row) const;
  void drawS60MenuIcon(int x, int y, const String &kind, uint16_t bg);
};

// New public name. Existing services retain SymbianUI as a source-compatibility adapter.
using VqeafUI = SymbianUI;
