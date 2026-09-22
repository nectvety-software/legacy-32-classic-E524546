#ifndef POCHITA_SYMBIAN_UI_H
#define POCHITA_SYMBIAN_UI_H

#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include "Display.h"
#include "UILayout.h"

extern TFT_eSPI tft;

// ---- OS-level status bar configuration ---------------------------------------
// Battery: set BATTERY_ADC_PIN to an ADC-capable GPIO to sense real battery
// voltage, or leave -1 to fall back to a cosmetic static bar.
#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN (-1)
#endif
#ifndef BATTERY_ADC_FULL
#define BATTERY_ADC_FULL 3400    // raw analogRead() at 100%
#endif
#ifndef BATTERY_ADC_EMPTY
#define BATTERY_ADC_EMPTY 2600   // raw analogRead() at 0%
#endif
#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif
#ifndef NTP_OFFSET_SEC
#define NTP_OFFSET_SEC (7 * 3600)  // UTC+7
#endif

// Shared 240x320 visual language inspired by classic S60 list screens.
// Keep drawing primitives cheap: this UI is rendered directly to the ST7789.
namespace SymbianUI {
// POCHITA OS pixel/retro visual language: a pure-black canvas, a single amber
// accent and white pixel text. Selected surfaces flip to a solid amber fill
// with dark text, echoing classic handheld-console menus.
constexpr uint16_t BG = TFT_BLACK;
constexpr uint16_t FG = TFT_WHITE;
constexpr uint16_t ACCENT = 0xFD80;         // amber / gold highlight
constexpr uint16_t ACCENT_DK = 0xC300;      // dim amber (scrollbar track)
constexpr uint16_t DIM = 0xC618;            // secondary label text
constexpr uint16_t MUTED = 0x4208;          // faint fill / scroll track
constexpr uint16_t DIVIDER = 0x8410;        // unselected card border / dots
constexpr uint16_t ON_ACCENT = TFT_BLACK;   // text/icon drawn over amber
constexpr uint16_t SELECT = ACCENT;         // selected background
constexpr uint16_t SELECT_BORDER = ACCENT;  // selected border
constexpr uint16_t GOOD = 0x07E0;
constexpr uint16_t WARN = 0xFD20;
constexpr uint16_t BAD = 0xF800;

constexpr int STATUS_H = 10;
constexpr int TITLE_H = 14;
constexpr int HEADER_H = STATUS_H + TITLE_H;  // 24px amber band, matches footer
constexpr int CONTENT_Y = HEADER_H + 2;
constexpr int LIST_ROW_H = 32;

enum Icon : uint8_t {
  ICON_NONE,
  ICON_WIFI,
  ICON_BLUETOOTH,
  ICON_FOLDER,
  ICON_FILE,
  ICON_TERMINAL,
  ICON_LORA,
  ICON_IR,
  ICON_SETTINGS,
  ICON_BROWSER,
  ICON_INFO,
  ICON_SEARCH,
  ICON_LOCK,
  ICON_DISPLAY,
  ICON_SYSTEM,
  ICON_BOOKMARK,
  ICON_HISTORY
};

inline String clockText() {
  time_t now = time(nullptr);
  if (now > 1700000000) {
    struct tm tmNow;
    localtime_r(&now, &tmNow);
    char value[6];
    strftime(value, sizeof(value), "%H:%M", &tmNow);
    return String(value);
  }
  unsigned long minutes = millis() / 60000UL;
  char value[6];
  snprintf(value, sizeof(value), "%02lu:%02lu", (minutes / 60UL) % 24UL,
           minutes % 60UL);
  return String(value);
}

// Cached battery percentage. Reads the ADC every ~5 s to avoid loading the
// display path; returns a cosmetic 65 when no battery sensor is configured.
inline int batteryPercent() {
  static int cached = -1;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (cached < 0 || now - last >= 5000) {
    last = now;
    int pin = BATTERY_ADC_PIN;
    if (pin < 0) {
      cached = 65;
    } else {
      int raw = analogRead(pin);
      cached = constrain((raw - BATTERY_ADC_EMPTY) * 100 /
                             (BATTERY_ADC_FULL - BATTERY_ADC_EMPTY),
                         0, 100);
    }
  }
  return cached;
}

// Arm the SNTP client. Harmless when WiFi is down; it auto-syncs as soon as a
// link appears, so a single call at boot covers every later connection.
inline void osSyncTime() {
  configTime(NTP_OFFSET_SEC, 0, NTP_SERVER);
  Serial.printf("[OS] NTP sync armed (%s, UTC%+d)\n", NTP_SERVER,
                NTP_OFFSET_SEC / 3600);
}

inline void drawWifiGlyph(int x, int y, uint16_t color) {
  tft.drawLine(x, y + 4, x + 6, y, color);
  tft.drawLine(x + 6, y, x + 12, y + 4, color);
  tft.drawLine(x + 3, y + 7, x + 6, y + 4, color);
  tft.drawLine(x + 6, y + 4, x + 9, y + 7, color);
  tft.fillCircle(x + 6, y + 10, 1, color);
}

inline void drawIcon(Icon icon, int x, int y, uint16_t color = FG) {
  switch (icon) {
    case ICON_WIFI:
      drawWifiGlyph(x + 1, y + 3, color);
      break;
    case ICON_BLUETOOTH:
      tft.drawFastVLine(x + 7, y + 2, 16, color);
      tft.drawLine(x + 7, y + 2, x + 13, y + 7, color);
      tft.drawLine(x + 13, y + 7, x + 3, y + 15, color);
      tft.drawLine(x + 3, y + 5, x + 13, y + 13, color);
      tft.drawLine(x + 13, y + 13, x + 7, y + 18, color);
      break;
    case ICON_FOLDER:
      tft.drawRect(x + 1, y + 6, 17, 11, color);
      tft.drawLine(x + 2, y + 5, x + 8, y + 5, color);
      tft.drawLine(x + 8, y + 5, x + 10, y + 7, color);
      break;
    case ICON_FILE:
      tft.drawRect(x + 3, y + 2, 13, 17, color);
      tft.drawLine(x + 10, y + 2, x + 16, y + 8, color);
      tft.drawFastHLine(x + 6, y + 11, 7, color);
      tft.drawFastHLine(x + 6, y + 14, 7, color);
      break;
    case ICON_TERMINAL:
      tft.drawRect(x + 1, y + 3, 18, 15, color);
      tft.drawLine(x + 4, y + 7, x + 7, y + 10, color);
      tft.drawLine(x + 7, y + 10, x + 4, y + 13, color);
      tft.drawFastHLine(x + 10, y + 13, 5, color);
      break;
    case ICON_SETTINGS:
      tft.drawCircle(x + 10, y + 10, 7, color);
      tft.drawCircle(x + 10, y + 10, 2, color);
      tft.drawFastVLine(x + 10, y, 3, color);
      tft.drawFastVLine(x + 10, y + 18, 3, color);
      tft.drawFastHLine(x, y + 10, 3, color);
      tft.drawFastHLine(x + 18, y + 10, 3, color);
      break;
    case ICON_SEARCH:
      tft.drawCircle(x + 8, y + 8, 6, color);
      tft.drawLine(x + 12, y + 13, x + 18, y + 19, color);
      break;
    case ICON_LOCK:
      tft.drawRect(x + 4, y + 9, 13, 10, color);
      tft.drawCircle(x + 10, y + 8, 5, color);
      break;
    case ICON_DISPLAY:
      tft.drawRect(x + 1, y + 2, 18, 14, color);
      tft.drawFastHLine(x + 6, y + 19, 8, color);
      tft.drawFastVLine(x + 10, y + 16, 3, color);
      break;
    case ICON_SYSTEM:
      tft.drawRect(x + 3, y + 3, 14, 14, color);
      for (int i = 5; i <= 15; i += 5) {
        tft.drawFastVLine(x, y + i, 3, color);
        tft.drawFastVLine(x + 18, y + i, 3, color);
        tft.drawFastHLine(x + i, y, 3, color);
        tft.drawFastHLine(x + i, y + 18, 3, color);
      }
      break;
    case ICON_BOOKMARK:
      tft.drawRect(x + 4, y + 2, 12, 17, color);
      tft.drawLine(x + 4, y + 19, x + 10, y + 14, color);
      tft.drawLine(x + 10, y + 14, x + 16, y + 19, color);
      break;
    case ICON_HISTORY:
      tft.drawCircle(x + 10, y + 10, 8, color);
      tft.drawLine(x + 10, y + 10, x + 10, y + 5, color);
      tft.drawLine(x + 10, y + 10, x + 15, y + 12, color);
      break;
    case ICON_INFO:
      tft.drawCircle(x + 10, y + 10, 8, color);
      tft.fillCircle(x + 10, y + 6, 1, color);
      tft.drawFastVLine(x + 10, y + 9, 7, color);
      break;
    case ICON_BROWSER:
      tft.drawCircle(x + 10, y + 10, 9, color);
      tft.drawFastHLine(x + 2, y + 10, 17, color);
      tft.drawFastVLine(x + 10, y + 2, 17, color);
      break;
    case ICON_LORA:
      tft.drawFastVLine(x + 10, y + 9, 11, color);
      tft.drawLine(x + 4, y + 12, x + 1, y + 9, color);
      tft.drawLine(x + 1, y + 9, x + 4, y + 6, color);
      tft.drawLine(x + 16, y + 6, x + 19, y + 9, color);
      tft.drawLine(x + 19, y + 9, x + 16, y + 12, color);
      break;
    case ICON_IR:
      tft.fillCircle(x + 5, y + 10, 2, color);
      tft.drawCircle(x + 5, y + 10, 6, color);
      tft.drawCircle(x + 5, y + 10, 10, color);
      break;
    default:
      break;
  }
}

// ---- Low-level pixel helpers ------------------------------------------

inline void drawDottedHLine(int x, int y, int w, uint16_t color, int step = 4) {
  for (int i = 0; i < w; i += step) tft.drawPixel(x + i, y, color);
}

// L-shaped accent brackets used to highlight a selected tile / card.
inline void drawCornerBrackets(int x, int y, int w, int h, uint16_t color,
                               int len = 9) {
  tft.drawFastHLine(x, y, len, color);
  tft.drawFastVLine(x, y, len, color);
  tft.drawFastHLine(x + w - len, y, len, color);
  tft.drawFastVLine(x + w - 1, y, len, color);
  tft.drawFastHLine(x, y + h - 1, len, color);
  tft.drawFastVLine(x, y + h - len, len, color);
  tft.drawFastHLine(x + w - len, y + h - 1, len, color);
  tft.drawFastVLine(x + w - 1, y + h - len, len, color);
}

// Small outlined pixel badge, e.g. "8BIT", "GB", "JSON".
inline void drawTag(int rightX, int cy, const String &label, uint16_t color,
                    uint16_t textColor) {
  int tw = label.length() * 6 + 12;
  int th = 16;
  int x = rightX - tw;
  int y = cy - th / 2;
  tft.drawRoundRect(x, y, tw, th, 3, color);
  tft.setTextColor(textColor);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + tw / 2, y + th / 2, 1);
}

inline void drawPixelBattery(int rightX, int cy, int battery, uint16_t color) {
  battery = constrain(battery, 0, 100);
  const int bw = 24, bh = 13;
  int x = rightX - bw, y = cy - bh / 2;
  tft.drawRoundRect(x, y, bw, bh, 2, color);
  tft.fillRect(x + bw, y + 4, 2, bh - 8, color);  // terminal nub
  int fill = (battery * 4 + 50) / 100;            // 0..4 cells
  for (int i = 0; i < fill; ++i)
    tft.fillRect(x + 3 + i * 4, y + 3, 3, bh - 6, color);
}

// Rounded card used by list rows / tiles. Selected = solid amber fill.
inline void drawRoundCard(int x, int y, int w, int h, bool selected) {
  if (selected) {
    tft.fillRoundRect(x, y, w, h, 6, ACCENT);
  } else {
    tft.fillRoundRect(x, y, w, h, 6, BG);
    tft.drawRoundRect(x, y, w, h, 6, DIVIDER);
  }
}

inline void drawStatusBar(const char *network = nullptr, int battery = -1) {
  // Single amber header band spanning the whole width.
  tft.fillRect(0, 0, UiLayout::WIDTH, HEADER_H, ACCENT);
  int b = battery >= 0 ? battery : batteryPercent();
  drawPixelBattery(UiLayout::WIDTH - 10, HEADER_H / 2, b, ON_ACCENT);

  // Real-time clock (real RTC time once NTP syncs, uptime until then).
  tft.setTextColor(ON_ACCENT);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(clockText(), UiLayout::WIDTH - 44, HEADER_H / 2, 1);

  // Live WiFi indicator (the "network" arg is kept for older callers).
  (void)network;
  bool wifiUp = (WiFi.status() == WL_CONNECTED);
  if (wifiUp) {
    drawWifiGlyph(UiLayout::WIDTH - 58, HEADER_H / 2 - 4, ON_ACCENT);
  }
}

// Cheap repaint of the status bar only (clock/battery/WiFi), for idle refresh.
inline void refreshStatusBar() {
  drawStatusBar();
}

inline void drawTitle(const String &title) {
  // The amber band is painted by drawStatusBar(); overlay the screen title
  // left-aligned in bold uppercase pixels.
  String up = title;
  up.toUpperCase();
  tft.setTextColor(ON_ACCENT);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(UiLayout::ellipsize(up, 20), 8, HEADER_H / 2, 2);
}


inline void drawSoftkeys(const String &left, const String &right,
                         const String &middle = "") {
  constexpr int y = UiLayout::FOOTER_Y;
  tft.fillRect(0, y, UiLayout::WIDTH, UiLayout::FOOTER_H, BG);
  drawDottedHLine(6, y + 3, UiLayout::WIDTH - 12, DIVIDER, 4);
  const int ty = y + 14;
  tft.setTextColor(FG);
  if (left.length()) {
    tft.setTextDatum(ML_DATUM);
    tft.drawString(UiLayout::ellipsize(left, 12), 10, ty, 2);
  }
  if (middle.length()) {
    tft.setTextDatum(MC_DATUM);
    tft.drawString(UiLayout::ellipsize(middle, 12), UiLayout::CENTER_X, ty, 2);
  }
  if (right.length()) {
    tft.setTextDatum(MR_DATUM);
    tft.drawString(UiLayout::ellipsize(right, 10), UiLayout::WIDTH - 10, ty, 2);
  }
}


inline void drawChrome(const String &title, const String &left,
                       const String &right) {
  tft.fillScreen(BG);
  drawStatusBar();
  drawTitle(title);
  drawSoftkeys(left, right);
}

inline void drawListRow(int y, int h, const String &label, bool selected,
                        const String &value = "", Icon icon = ICON_NONE) {
  const int x = 4;
  const int w = UiLayout::WIDTH - 8;
  const int gap = 3;
  int cardY = y + gap;
  int cardH = h - gap - 1;
  // Clear the whole row band first to avoid ghosting between redraws.
  tft.fillRect(x, y, w, h, BG);
  drawRoundCard(x, cardY, w, cardH, selected);

  uint16_t txt = selected ? ON_ACCENT : FG;
  uint16_t sub = selected ? ON_ACCENT : DIM;
  int cy = cardY + cardH / 2;

  int textX = x + 12;
  if (icon != ICON_NONE) {
    drawIcon(icon, x + 8, cy - 10, txt);
    textX = x + 36;
  }
  tft.setTextColor(txt);
  tft.setTextDatum(ML_DATUM);
  size_t maxChars = value.length() ? (icon == ICON_NONE ? 16 : 13)
                                   : (icon == ICON_NONE ? 26 : 21);
  tft.drawString(UiLayout::ellipsize(label, maxChars), textX, cy, 2);
  if (value.length()) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(sub);
    tft.drawString(UiLayout::ellipsize(value, 10), x + w - 12, cy, 2);
  }
}


inline void drawSectionLabel(int y, const String &label) {
  tft.fillRect(0, y, UiLayout::WIDTH, 18, BG);
  String up = label;
  up.toUpperCase();
  tft.setTextColor(ACCENT, BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(UiLayout::ellipsize(up, 28), 8, y + 9, 2);
  drawDottedHLine(6, y + 17, UiLayout::WIDTH - 12, DIVIDER, 4);
}

inline void drawInfoLine(int y, const String &label, const String &value) {
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(DIM, BG);
  tft.drawString(label, 8, y, 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(FG, BG);
  tft.drawString(UiLayout::ellipsize(value, 20), 232, y, 2);
}

inline void drawProgressBar(int x, int y, int w, int value,
                            uint16_t color = ACCENT) {
  value = constrain(value, 0, 100);
  const int h = 14;
  tft.fillRoundRect(x, y, w, h, 4, BG);
  tft.drawRoundRect(x, y, w, h, 4, color);
  int fill = ::map(value, 0, 100, 0, w - 6);
  if (fill > 0) tft.fillRoundRect(x + 3, y + 3, fill, h - 6, 2, color);
}


inline void drawEmptyState(Icon icon, const String &title,
                           const String &detail = "") {
  drawIcon(icon, 110, 118, ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FG, BG);
  tft.drawString(title, 120, 154, 2);
  if (detail.length()) {
    tft.setTextColor(DIM, BG);
    tft.drawString(UiLayout::ellipsize(detail, 30), 120, 176, 2);
  }
}

inline void drawFormField(int y, const String &label, const String &value,
                          bool selected = false, bool masked = false) {
  tft.setTextColor(ACCENT, BG);
  tft.setTextDatum(ML_DATUM);
  String up = label;
  up.toUpperCase();
  tft.drawString(UiLayout::ellipsize(up, 28), 8, y, 2);
  const int by = y + 12, bw = 228, bh = 30;
  tft.fillRoundRect(6, by, bw, bh, 6, BG);
  tft.drawRoundRect(6, by, bw, bh, 6, selected ? ACCENT : DIVIDER);
  if (selected) tft.drawRoundRect(7, by + 1, bw - 2, bh - 2, 6, ACCENT);
  String shown = value;
  if (masked) {
    shown = "";
    for (size_t i = 0; i < value.length(); ++i) shown += '*';
  }
  if (!shown.length()) shown = "-";
  tft.setTextColor(shown == "-" ? DIM : FG, BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(UiLayout::ellipsize(shown, 26), 14, by + bh / 2, 2);
}

inline void drawMessageScreen(const String &screenTitle, Icon icon,
                              const String &headline, const String &detail,
                              const String &left = "", const String &right = "Back",
                              uint16_t tone = ACCENT) {
  drawChrome(screenTitle, left, right);
  drawIcon(icon, 110, 108, tone);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tone, BG);
  tft.drawString(UiLayout::ellipsize(headline, 24), 120, 148, 2);
  tft.setTextColor(DIM, BG);
  tft.drawString(UiLayout::ellipsize(detail, 30), 120, 172, 2);
}

inline void drawDialog(const String &title, const String &message,
                       const String &left, const String &right,
                       bool leftSelected = true, Icon icon = ICON_INFO,
                       uint16_t tone = ACCENT) {
  (void)icon;
  const int x = 14, y = 92, w = 212, h = 128;
  tft.fillRoundRect(x, y, w, h, 8, BG);
  tft.drawRoundRect(x, y, w, h, 8, tone);
  tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 8, tone);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tone, BG);
  tft.drawString(UiLayout::ellipsize(title, 20), x + w / 2, y + 22, 2);
  tft.setTextColor(FG, BG);
  tft.drawString(UiLayout::ellipsize(message, 26), x + w / 2, y + 54, 2);

  const int btnY = y + 82, btnW = 84, btnH = 30;
  const int leftX = x + 14, rightX = x + w - btnW - 14;
  if (leftSelected)
    tft.fillRoundRect(leftX, btnY, btnW, btnH, 6, ACCENT);
  else
    tft.drawRoundRect(leftX, btnY, btnW, btnH, 6, DIVIDER);
  if (!leftSelected)
    tft.fillRoundRect(rightX, btnY, btnW, btnH, 6, ACCENT);
  else
    tft.drawRoundRect(rightX, btnY, btnW, btnH, 6, DIVIDER);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(leftSelected ? ON_ACCENT : FG);
  tft.drawString(UiLayout::ellipsize(left, 10), leftX + btnW / 2,
                 btnY + btnH / 2, 2);
  tft.setTextColor(leftSelected ? FG : ON_ACCENT);
  tft.drawString(UiLayout::ellipsize(right, 10), rightX + btnW / 2,
                 btnY + btnH / 2, 2);
}

}  // namespace SymbianUI

#endif
