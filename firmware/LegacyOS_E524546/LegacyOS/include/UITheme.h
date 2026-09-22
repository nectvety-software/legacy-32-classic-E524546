#pragma once
#include <TFT_eSPI.h>

// ═══════════════════════════════════════════════════════════
//  LegacyOS UI Theme - Android Material You Style
// ═══════════════════════════════════════════════════════════

// ─── Color Palette (16-bit RGB565) ──────────────────────────
#define CLR_BG          0x0841   // #101020 - Deep dark blue-black
#define CLR_BG2         0x10A3   // #201040 - Card background
#define CLR_SURFACE     0x2104   // #400810 - Surface
#define CLR_PRIMARY     0x633F   // #C267FF - Purple accent (Material You)
#define CLR_SECONDARY   0x3B9F   // #773FFF - Secondary purple
#define CLR_ACCENT      0xFF20   // #FF4004 - Red accent
#define CLR_GREEN       0x07E0   // #00FF00 - Success green
#define CLR_CYAN        0x07FF   // #00FFFF - Info cyan
#define CLR_YELLOW      0xFFE0   // #FFFF00 - Warning yellow
#define CLR_ORANGE      0xFB40   // #FF6800 - Orange
#define CLR_WHITE       0xFFFF   // #FFFFFF
#define CLR_GRAY1       0xC618   // #C0C0C0 - Light gray
#define CLR_GRAY2       0x7BEF   // #787878 - Mid gray
#define CLR_GRAY3       0x39E7   // #383838 - Dark gray
#define CLR_BLACK       0x0000   // #000000
#define CLR_TRANSPARENT 0x0841   // Same as BG

// Status bar colors
#define CLR_STATUS_BG   0x0000   // Black status bar
#define CLR_STATUS_TXT  0xFFFF   // White text

// App icon background colors
#define CLR_APP_RED     0xF800
#define CLR_APP_BLUE    0x001F
#define CLR_APP_GREEN   0x03E0
#define CLR_APP_PURPLE  0x780F
#define CLR_APP_ORANGE  0xFC00
#define CLR_APP_TEAL    0x0410
#define CLR_APP_PINK    0xF81F
#define CLR_APP_YELLOW  0xFFE0

// ─── Font sizes ─────────────────────────────────────────────
#define FONT_SMALL    1   // 6x8
#define FONT_MEDIUM   2   // 12x16
#define FONT_LARGE    4   // 26x32

// ─── UI Constants ───────────────────────────────────────────
#define CORNER_RADIUS     8
#define CARD_PADDING      6
#define ICON_SIZE        40
#define ICON_GRID_COLS    4
#define ICON_GRID_ROWS    3
#define ICON_SPACING_X   58
#define ICON_SPACING_Y   62
#define ICON_START_X     10
#define ICON_START_Y     30

// ─── UITheme class ───────────────────────────────────────────
class UITheme {
public:
  enum class ThemeMode { DARK, LIGHT, AMOLED };
  
  ThemeMode mode = ThemeMode::AMOLED;
  
  uint16_t bg()       { return mode == ThemeMode::LIGHT ? 0xF7BE : CLR_BG; }
  uint16_t surface()  { return mode == ThemeMode::LIGHT ? 0xFFFF : CLR_BG2; }
  uint16_t primary()  { return CLR_PRIMARY; }
  uint16_t text()     { return mode == ThemeMode::LIGHT ? 0x0000 : CLR_WHITE; }
  uint16_t textDim()  { return mode == ThemeMode::LIGHT ? 0x4208 : CLR_GRAY2; }
  uint16_t accent()   { return CLR_ACCENT; }
  uint16_t success()  { return CLR_GREEN; }
  uint16_t warning()  { return CLR_YELLOW; }
  uint16_t error()    { return CLR_ACCENT; }
  
  // Draw rounded rectangle helper
  static void drawRoundRect(TFT_eSPI* tft, int x, int y, int w, int h, 
                            int r, uint16_t color, bool fill = false) {
    if (fill) {
      tft->fillRoundRect(x, y, w, h, r, color);
    } else {
      tft->drawRoundRect(x, y, w, h, r, color);
    }
  }

  // Draw card
  static void drawCard(TFT_eSPI* tft, int x, int y, int w, int h, 
                       uint16_t bgColor, uint16_t borderColor = 0) {
    tft->fillRoundRect(x, y, w, h, CORNER_RADIUS, bgColor);
    if (borderColor) {
      tft->drawRoundRect(x, y, w, h, CORNER_RADIUS, borderColor);
    }
  }
  
  // Draw app icon with label
  static void drawAppIcon(TFT_eSPI* tft, int x, int y, 
                          const char* emoji, const char* label,
                          uint16_t iconBg, bool selected = false) {
    uint16_t border = selected ? CLR_WHITE : 0;
    if (selected) {
      tft->fillRoundRect(x-2, y-2, ICON_SIZE+4, ICON_SIZE+4, 10, CLR_PRIMARY);
    }
    tft->fillRoundRect(x, y, ICON_SIZE, ICON_SIZE, 10, iconBg);
    if (border) tft->drawRoundRect(x, y, ICON_SIZE, ICON_SIZE, 10, border);
    
    // Center emoji/text
    tft->setTextColor(CLR_WHITE);
    tft->setTextSize(2);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(emoji, x + ICON_SIZE/2, y + ICON_SIZE/2);
    
    // Label below
    tft->setTextSize(1);
    tft->setTextColor(CLR_WHITE);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(label, x + ICON_SIZE/2, y + ICON_SIZE + 6);
  }
  
  // Progress bar
  static void drawProgressBar(TFT_eSPI* tft, int x, int y, int w, int h,
                              int percent, uint16_t fgColor, uint16_t bgColor) {
    tft->fillRoundRect(x, y, w, h, h/2, bgColor);
    int filled = (w * percent) / 100;
    if (filled > 0) {
      tft->fillRoundRect(x, y, filled, h, h/2, fgColor);
    }
  }
  
  // Battery icon
  static void drawBattery(TFT_eSPI* tft, int x, int y, int percent, bool charging) {
    uint16_t c = percent > 20 ? CLR_GREEN : CLR_ACCENT;
    tft->drawRect(x, y, 14, 8, CLR_WHITE);
    tft->drawRect(x+14, y+2, 2, 4, CLR_WHITE);
    int fill = (12 * percent) / 100;
    if (fill > 0) tft->fillRect(x+1, y+1, fill, 6, c);
    if (charging) {
      tft->setTextColor(CLR_YELLOW);
      tft->setTextSize(1);
      tft->drawChar(x+3, y, '+', CLR_YELLOW, CLR_STATUS_BG, 1);
    }
  }
  
  // WiFi signal bars
  static void drawWifiIcon(TFT_eSPI* tft, int x, int y, int strength) {
    // strength: 0-4
    uint16_t c = CLR_WHITE;
    for (int i = 0; i < 4; i++) {
      uint16_t barC = (i < strength) ? c : CLR_GRAY3;
      int bh = 2 + i * 2;
      tft->fillRect(x + i*4, y + (8-bh), 3, bh, barC);
    }
  }

  // Scrollbar
  static void drawScrollbar(TFT_eSPI* tft, int x, int y, int h, 
                            int total, int visible, int offset) {
    if (total <= visible) return;
    tft->fillRect(x, y, 3, h, CLR_GRAY3);
    int barH = (h * visible) / total;
    int barY = y + (h * offset) / total;
    tft->fillRect(x, barY, 3, barH, CLR_PRIMARY);
  }
};

// Global theme instance
extern UITheme gTheme;
