#include "SymbianUI.h"
#include "BoardConfig.h"
#include <time.h>
#include <WiFi.h>
#include "UiVietnameseFont.h"
#include "UiTypography.h"
#include "UiIconCatalog.h"
#include "VqeafIconRenderer.h"

// C++11 out-of-class definitions for S60 geometry constants.
constexpr int SymbianUI::TITLEBAR_H;
constexpr int SymbianUI::CONTENT_TOP;
constexpr int SymbianUI::SOFTKEY_H;
constexpr int SymbianUI::SOFTKEY_TOP;
constexpr int SymbianUI::LIST_ROW_H;
constexpr int SymbianUI::LIST_VISIBLE;
constexpr int SymbianUI::GRID_COLS;
constexpr int SymbianUI::GRID_ROWS;
constexpr int SymbianUI::GRID_CELL_W;
constexpr int SymbianUI::GRID_CELL_H;
constexpr int SymbianUI::ICON_BOX;
constexpr int SymbianUI::STATUS_ZONE_W;
constexpr int SymbianUI::STATUS_ICON_W;
constexpr int SymbianUI::STATUS_ICON_GAP;
constexpr int SymbianUI::STATUS_RIGHT_PAD;

void SymbianUI::begin() {
  pinMode(Board::TFT_LEDK_PIN, OUTPUT);
  digitalWrite(Board::TFT_LEDK_PIN, HIGH);
  tft.init();
  tft.setRotation(Board::TFT_ROTATION);
  tft.setTextWrap(false);
  tft.setTextFont(1);
  clear();
}


int SymbianUI::textWidth(const String &text, uint8_t font) {
  const uint8_t oldFont = font;
  tft.setTextFont(oldFont);
  tft.setTextSize(1);
  return UiVietnameseFont::hasUtf8(text.c_str())
    ? UiVietnameseFont::measure(text.c_str(), font == UiTypography::MICRO ? 0 : 1)
    : tft.textWidth(text);
}

void SymbianUI::textBold(int x, int y, const String &text, uint8_t font,
                         uint16_t fg, uint16_t bg, bool extraBold) {
  // Nokia 2700/S40-inspired heavy raster text without loading a custom font.
  // TFT_eSPI Font 2 is used for UI labels and drawn twice one pixel apart.
  // This costs no font RAM/PSRAM and keeps the exact same baseline/metrics.
  if (UiVietnameseFont::hasUtf8(text.c_str())) {
    UiVietnameseFont::draw(tft, x, y, text.c_str(), fg, bg,
                           font == UiTypography::MICRO ? 0 : 1, Board::SCREEN_W - x);
    return;
  }
  tft.setTextFont(font);
  tft.setTextSize(1);
  tft.setTextColor(fg, bg);
  tft.setCursor(x, y);
  tft.print(text);
  // Subsequent opaque passes repaint their background and ERASE the first
  // stroke. Keep only the base pass opaque; overlay strokes are transparent.
  tft.setTextColor(fg);
  tft.setCursor(x + 1, y);
  tft.print(text);
  if (extraBold) { tft.setCursor(x, y + 1); tft.print(text); }
  tft.setTextColor(fg, bg);
}

void SymbianUI::setExternalTheme(const ThemeColors &palette, const LauncherStyle *skin) {
  themeId = ThemeId::External;
  colors = palette;
  launcherSkin = skin ? *skin : LauncherStyle::fromPalette(palette);
  // One theme affects physical LCD chrome, content, cards, selection & footer.
  // Studio's virtual phoneShell/keypad resources are not drawn over the LCD.
  colors.bg = launcherSkin.listBg;
  colors.text = launcherSkin.listFg;
  colors.selected = launcherSkin.selectedBg;
  selectedInk = launcherSkin.selectedFg;
  colors.chrome = launcherSkin.headerBg;
  colors.chromeText = launcherSkin.headerFg;
  colors.panel = launcherSkin.previewBg;
  colors.popup = launcherSkin.previewBg;
  colors.popupText = launcherSkin.previewFg;
  colors.accent = launcherSkin.tabAccent;
  colors.border = launcherSkin.border;
  invalidateChrome();
}

void SymbianUI::setTheme(ThemeId id) {
  themeId = id;
  colors = themeFor(id);
  selectedInk = colors.text;
  launcherSkin = LauncherStyle::fromPalette(colors);
  invalidateChrome();
}

void SymbianUI::invalidateChrome() {
  chromeValid = false;
  softkeysValid = false;
  chromeTitle[0] = 0;
  chromeClock[0] = 0;
  chromeWifiBars = 0;
  softLeft[0] = softCenter[0] = softRight[0] = 0;
}

void SymbianUI::clear() {
  tft.fillScreen(colors.bg);
  invalidateChrome();
}

void SymbianUI::clearContent() {
  tft.fillRect(0, CONTENT_TOP, Board::SCREEN_W, SOFTKEY_TOP - CONTENT_TOP, colors.bg);
}

void SymbianUI::clearListRow(int row) {
  if (row < 0 || row >= LIST_VISIBLE) return;
  const int y = CONTENT_TOP + 1 + row * LIST_ROW_H;
  tft.fillRect(2, y, Board::SCREEN_W - 7, LIST_ROW_H - 1, colors.bg);
}

String SymbianUI::fitTextPixels(const String &label,uint8_t font,int maxPx) {
  if(maxPx<=0)return String();
  if(textWidth(label,font)<=maxPx)return label;
  String cut=label;
  while(cut.length() && textWidth(cut+"~",font)>maxPx) {
    // Remove the entire final UTF-8 codepoint, not merely its trailing byte.
    // NFC input is required by the tiny bitmap font; malformed input is '?' in draw.
    size_t first=cut.length()-1;
    while(first>0 && (((uint8_t)cut[first]&0xC0)==0x80))--first;
    cut.remove(first);
  }
  return cut.length()?cut+"~":String();
}

String SymbianUI::timeText(bool hour12) {
  struct tm info;
  if (getLocalTime(&info, 5)) {
    char b[12];
    if (hour12) strftime(b, sizeof(b), "%I:%M %p", &info);
    else strftime(b, sizeof(b), "%H:%M", &info);
    return String(b);
  }
  // Unlike uptime, an unsynchronized wall clock is not an actual time.
  // This board has no RTC: show an explicit placeholder until NTP succeeds.
  return String("--:--");
}

void SymbianUI::pixelLineH(int x, int y, int w, uint16_t c, int s) {
  tft.fillRect(x, y, w, s, c);
}

void SymbianUI::pixelLineV(int x, int y, int h, uint16_t c, int s) {
  tft.fillRect(x, y, s, h, c);
}

uint8_t SymbianUI::statusWifiBars() const {
  if (WiFi.status() != WL_CONNECTED) return 0;
  const int rssi = WiFi.RSSI();
  return rssi >= -55 ? 4 : (rssi >= -67 ? 3 : (rssi >= -78 ? 2 : 1));
}

void SymbianUI::drawStatusWifi(int x, int y, uint16_t c) {
  // Feature-phone style WiFi bars. The caller clears the complete status slot
  // before redraw, so a weaker RSSI never leaves stale taller bars behind.
  const uint8_t bars = statusWifiBars();
  const int heights[4] = {2,4,6,8};
  for (int i=0;i<4;++i) {
    if (i < bars) tft.fillRect(x + i*3, y + 9 - heights[i], 2, heights[i], c);
  }
}

void SymbianUI::drawStatusBle(int x, int y, uint16_t c) {
  pixelLineV(x + 4, y, 10, c, 1);
  tft.drawLine(x + 4, y, x + 8, y + 3, c);
  tft.drawLine(x + 8, y + 3, x + 2, y + 7, c);
  tft.drawLine(x + 2, y + 2, x + 8, y + 7, c);
  tft.drawLine(x + 8, y + 7, x + 4, y + 10, c);
}

void SymbianUI::drawStatusSd(int x, int y, uint16_t c) {
  tft.drawRect(x, y, 8, 10, c);
  tft.fillRect(x + 2, y + 1, 4, 2, c);
}

void SymbianUI::drawStatusBattery(int x, int y, uint16_t c) {
  tft.drawRect(x, y + 1, 9, 7, c);
  tft.fillRect(x + 9, y + 3, 2, 3, c);
  // No battery ADC in the confirmed BOM: outline only, never fake charge bars.
}

void SymbianUI::chrome(const String &title, bool wifi, bool ble, bool sd, bool hour12) {
  (void)ble; (void)sd;
  String tm = timeText(hour12);
  const uint8_t wifiBars = wifi ? statusWifiBars() : 0;
  const bool sameShell = chromeValid && title == chromeTitle &&
                         hour12 == chromeHour12;
  const uint16_t bar = colors.chrome;
  const uint16_t ink = colors.chromeText;

  // 240px titlebar: title | centered clock | compact WiFi+battery group.
  // The two status glyphs are deliberately close together at the far right.
  const int centerX = STATUS_ZONE_W;
  const int rightX = STATUS_ZONE_W * 2;
  const int iconY = 8;
  const int batteryX = Board::SCREEN_W - STATUS_RIGHT_PAD - STATUS_ICON_W;
  const int wifiX = batteryX - STATUS_ICON_GAP - STATUS_ICON_W;

  if (sameShell) {
    if (tm != chromeClock) {
      tft.fillRect(centerX, 0, STATUS_ZONE_W, TITLEBAR_H - 2, bar);
      tft.setTextColor(ink, bar); tft.setTextFont(1); tft.setTextSize(1);
      const int tw = tft.textWidth(tm);
      tft.setCursor((Board::SCREEN_W - tw) / 2, 9);
      tft.print(tm);
      snprintf(chromeClock, sizeof(chromeClock), "%s", tm.c_str());
    }
    if (wifiBars != chromeWifiBars) {
      tft.fillRect(rightX, 0, STATUS_ZONE_W, TITLEBAR_H - 2, bar);
      if (wifiBars) drawStatusWifi(wifiX, iconY, ink);
      drawStatusBattery(batteryX, iconY - 1, ink);
      chromeWifiBars = wifiBars;
    }
    chromeWifi = wifi;
    return;
  }

  tft.fillRect(0, 0, Board::SCREEN_W, TITLEBAR_H, bar);
  if (themeId == ThemeId::S60Green) {
    tft.drawFastHLine(0, TITLEBAR_H - 2, Board::SCREEN_W, 0x4BE5);
    tft.drawFastHLine(0, TITLEBAR_H - 1, Board::SCREEN_W, 0xAEE9);
  } else if (themeId == ThemeId::AmoledRed || themeId == ThemeId::External) {
    tft.drawFastHLine(0, TITLEBAR_H - 2, Board::SCREEN_W, colors.border);
    tft.drawFastHLine(0, TITLEBAR_H - 1, Board::SCREEN_W, colors.accent);
  } else {
    tft.drawFastHLine(0, TITLEBAR_H - 1, Board::SCREEN_W, 0x8410);
  }

  // Left zone: app title. Trim by rendered width rather than character count so
  // it can never collide with the centered time on the narrow 240px display.
  tft.setTextColor(ink, bar); tft.setTextFont(2); tft.setTextSize(1);
  String cut = title;
  const int titleMaxW = STATUS_ZONE_W - 8;
  while (cut.length() > 1 && tft.textWidth(cut) > titleMaxW) cut.remove(cut.length() - 1);
  if (cut != title && cut.length() > 1) {
    cut.remove(cut.length() - 1);
    cut += "~";
  }
  textBold(UiTypography::HEADER_TITLE_X,UiTypography::HEADER_TITLE_Y,cut,
           UiTypography::TITLE,ink,bar,true);

  // Center zone: clock is centered against the complete 240px screen.
  tft.setTextFont(1);
  const int tw = tft.textWidth(tm);
  tft.setCursor((Board::SCREEN_W - tw) / 2, 9);
  tft.print(tm);

  // Right zone: two equal 40px slots. This board has no cellular modem/SIM.
  // status area is intentionally WiFi + battery only.
  if (wifiBars) drawStatusWifi(wifiX, iconY, ink);
  drawStatusBattery(batteryX, iconY - 1, ink);

  chromeValid = true;
  chromeWifi = wifi; chromeBle = false; chromeSd = false; chromeHour12 = hour12;
  chromeWifiBars = wifiBars;
  snprintf(chromeTitle, sizeof(chromeTitle), "%s", title.c_str());
  snprintf(chromeClock, sizeof(chromeClock), "%s", tm.c_str());
}

void SymbianUI::refreshWifiBadge(bool connected, bool hour12) {
  // Preserve title/center clock when network changes on Menu or another app.
  if (!chromeValid) return;
  chrome(String(chromeTitle), connected, false, false, hour12);
}

void SymbianUI::softkeys(const String &left, const String &center, const String &right) {
  if (softkeysValid && left == softLeft && center == softCenter && right == softRight) return;
  const int y = SOFTKEY_TOP;
  const uint16_t bar = themeId == ThemeId::S60Green ? 0xB6EE :
                       (themeId == ThemeId::External ? launcherSkin.footerBg : colors.chrome);
  const uint16_t ink = themeId == ThemeId::S60Green ? 0x1A63 :
                       (themeId == ThemeId::External ? launcherSkin.footerFg : colors.chromeText);
  tft.fillRect(0, y, Board::SCREEN_W, SOFTKEY_H, bar);
  tft.drawFastHLine(0, y, Board::SCREEN_W, themeId == ThemeId::S60Green ? TFT_WHITE : colors.border);
  if (themeId == ThemeId::S60Green) {
    tft.drawFastHLine(0, y + 1, Board::SCREEN_W, 0x75A3);
    tft.drawFastVLine(Board::SCREEN_W/3, y+3, SOFTKEY_H-6, 0x96A6);
    tft.drawFastVLine((Board::SCREEN_W*2)/3, y+3, SOFTKEY_H-6, 0x96A6);
  } else if (themeId == ThemeId::AmoledRed || themeId == ThemeId::External) {
    tft.drawFastHLine(0, y + 1, Board::SCREEN_W, colors.accent);
    tft.drawFastVLine(Board::SCREEN_W/3, y+3, SOFTKEY_H-6, colors.border);
    tft.drawFastVLine((Board::SCREEN_W*2)/3, y+3, SOFTKEY_H-6, colors.border);
  }
  tft.setTextColor(ink, bar); tft.setTextFont(2); tft.setTextSize(1);
  textBold(4, y + 3, left, 2, ink, bar);
  int cw = tft.textWidth(center); textBold((Board::SCREEN_W - cw) / 2, y + 3, center, 2, ink, bar);
  int rw = tft.textWidth(right); textBold(Board::SCREEN_W - rw - 4, y + 3, right, 2, ink, bar);
  tft.setTextFont(1);
  softkeysValid = true;
  snprintf(softLeft, sizeof(softLeft), "%s", left.c_str());
  snprintf(softCenter, sizeof(softCenter), "%s", center.c_str());
  snprintf(softRight, sizeof(softRight), "%s", right.c_str());
}

static uint16_t themeMix565(uint16_t a, uint16_t b, int amount) {
  // Compact 0..8 weighted RGB565 blend, no heap or floating point.
  const int r = (((a >> 11) & 31) * (8-amount) + ((b >> 11) & 31) * amount) / 8;
  const int g = (((a >> 5) & 63) * (8-amount) + ((b >> 5) & 63) * amount) / 8;
  const int bl = ((a & 31) * (8-amount) + (b & 31) * amount) / 8;
  return uint16_t((r << 11) | (g << 5) | bl);
}

uint16_t SymbianUI::menuRowColor(int row) const {
  row = constrain(row, 0, 3);
  if (themeId == ThemeId::S60Green) {
    static const uint16_t greenRows[4] = {0x75A3, 0x8E25, 0x96A6, 0x8E44};
    return greenRows[row];
  }
  if (themeId == ThemeId::AmoledRed) {
    // Near-black row bands keep AMOLED pixels mostly off while a faint red tint
    // preserves the S60 grid structure without storing a wallpaper bitmap.
    static const uint16_t redRows[4] = {0x0000, 0x0801, 0x1001, 0x0800};
    return redRows[row];
  }
  if (themeId == ThemeId::External) return themeMix565(colors.bg, colors.panel, row + 1);
  return colors.bg;
}

void SymbianUI::menuBackground() {
  if (themeId != ThemeId::S60Green && themeId != ThemeId::AmoledRed && themeId != ThemeId::External) {
    clearContent();
    return;
  }
  // 3x4 wallpaper uses deterministic row colors so repainting one cell always
  // reconstructs the same pixels and D-pad navigation remains flicker-free.
  for (int row = 0; row < GRID_ROWS; ++row) {
    const int y = CONTENT_TOP + row * GRID_CELL_H;
    tft.fillRect(0, y, Board::SCREEN_W, GRID_CELL_H, menuRowColor(row));
    if (row) tft.drawFastHLine(0, y, Board::SCREEN_W,
      themeId == ThemeId::S60Green ? 0xAEE9 : colors.border);
  }
  if (themeId == ThemeId::AmoledRed) {
    // Low-cost red glow strokes inspired by the VQEAF frameFx/glow fields.
    for (int i=0;i<4;++i) {
      tft.drawLine(0, CONTENT_TOP+48+i*3, 170, CONTENT_TOP+180+i*5, colors.border);
      tft.drawLine(55, CONTENT_TOP+245-i*3, 238, CONTENT_TOP+116+i*4, 0x2801);
    }
  }
  const uint16_t track = themeId == ThemeId::S60Green ? 0xDF93 : colors.border;
  const uint16_t thumb = themeId == ThemeId::S60Green ? TFT_WHITE : colors.accent;
  tft.fillRect(Board::SCREEN_W - 4, CONTENT_TOP + 6, 2, GRID_ROWS * GRID_CELL_H - 12, track);
  tft.fillRect(Board::SCREEN_W - 4, CONTENT_TOP + 8, 2, 82, thumb);
  tft.fillRect(0, CONTENT_TOP + GRID_ROWS * GRID_CELL_H, Board::SCREEN_W,
               SOFTKEY_TOP - (CONTENT_TOP + GRID_ROWS * GRID_CELL_H), menuRowColor(3));
}

void SymbianUI::drawS60MenuIcon(int x, int y, const String &requestedKind, uint16_t bg) {
  const String kind = UiIconCatalog::canonical(requestedKind.c_str());
  const VqeafIcons::Id standardized = VqeafIcons::fromLegacy(kind.c_str());
  if (standardized != VqeafIcons::Id::Count) {
    VqeafIcons::draw(tft, standardized, x, y, 36, bg, VqeafIcons::Palette::standard());
    return;
  }
  // Preserve v2.3 procedural drawings for non-core icons.
  // 36x36 vector/pixel icons inspired by S60 3rd Edition. No image buffers or
  // decoded assets are retained in RAM; every icon is drawn directly to ST7789.
  tft.fillRect(x, y, ICON_BOX, ICON_BOX, bg);
  const uint16_t shadow = 0x4208;
  const uint16_t silver = 0xD69A;
  const uint16_t white = TFT_WHITE;
  const uint16_t black = TFT_BLACK;

  if (kind == "Dir") {
    tft.fillRect(x+5,y+11,27,19,shadow);
    tft.fillRect(x+3,y+9,27,19,0xFD20);
    tft.fillRect(x+6,y+6,12,6,0xFFE0);
    tft.drawRect(x+3,y+9,27,19,0x9A60);
    tft.drawFastHLine(x+5,y+10,23,white);
  } else if (kind == "Pic" || kind == "Col") {
    tft.fillRect(x+9,y+6,23,24,shadow);
    tft.fillRect(x+4,y+10,24,21,silver);
    tft.fillRect(x+7,y+12,18,15,0x4DDF);
    tft.fillTriangle(x+7,y+27,x+14,y+18,x+19,y+27,0x45E5);
    tft.fillTriangle(x+13,y+27,x+20,y+20,x+25,y+27,0x75E0);
    tft.fillCircle(x+21,y+16,2,0xFFE0);
    tft.drawRect(x+4,y+10,24,21,white);
  } else if (kind == "Web") {
    tft.fillCircle(x+18,y+18,13,0x04DF);
    tft.drawCircle(x+18,y+18,13,black);
    tft.fillCircle(x+14,y+14,4,0x4FE6);
    tft.fillRect(x+19,y+18,7,5,0x4FE6);
    tft.drawCircle(x+18,y+18,8,white);
    tft.drawFastHLine(x+6,y+18,24,white);
    tft.drawLine(x+5,y+27,x+30,y+8,0xFFE0);
    tft.fillCircle(x+30,y+8,2,0xFD20);
  } else if (kind == "Mus") {
    tft.drawFastVLine(x+20,y+5,18,0xF80F);
    tft.drawFastVLine(x+23,y+5,18,0xF80F);
    tft.fillRect(x+20,y+5,10,3,0xF80F);
    tft.fillCircle(x+15,y+26,6,0xF80F);
    tft.fillCircle(x+28,y+23,5,0xF80F);
    tft.fillCircle(x+27,y+27,7,silver);
    tft.fillTriangle(x+25,y+23,x+25,y+31,x+31,y+27,0x07E0);
  } else if (kind == "Wi") {
    for (int r=14;r>=6;r-=4) {
      tft.drawCircle(x+18,y+22,r,0x04FF);
      tft.fillRect(x+2,y+22,32,14,bg);
    }
    tft.fillCircle(x+18,y+27,4,0x04FF);
  } else if (kind == "BLE" || kind == "BT") {
    tft.fillRect(x+7,y+4,22,28,0x001F);
    tft.drawRect(x+7,y+4,22,28,white);
    tft.drawFastVLine(x+18,y+8,20,white);
    tft.drawLine(x+18,y+8,x+25,y+14,white);
    tft.drawLine(x+25,y+14,x+12,y+23,white);
    tft.drawLine(x+12,y+12,x+25,y+22,white);
    tft.drawLine(x+25,y+22,x+18,y+28,white);
  } else if (kind == "Term" || kind == "Sh") {
    tft.fillRect(x+4,y+6,29,25,shadow);
    tft.fillRect(x+2,y+4,29,25,0x1082);
    tft.fillRect(x+2,y+4,29,5,silver);
    tft.drawRect(x+2,y+4,29,25,white);
    tft.drawLine(x+7,y+14,x+12,y+19,white);
    tft.drawLine(x+12,y+19,x+7,y+24,white);
    tft.drawFastHLine(x+16,y+24,8,white);
  } else if (kind == "Rec") {
    tft.fillCircle(x+18,y+18,13,0x07A0);
    tft.drawCircle(x+18,y+18,13,0x01E0);
    tft.drawCircle(x+18,y+18,9,white);
    tft.fillTriangle(x+25,y+8,x+31,y+10,x+26,y+14,white);
    tft.fillTriangle(x+11,y+28,x+5,y+26,x+10,y+22,white);
    tft.fillRect(x+17,y+8,9,3,0x07A0);
    tft.fillRect(x+10,y+25,9,3,0x07A0);
  } else if (kind == "Set") {
    tft.fillCircle(x+18,y+18,12,silver);
    for (int a=0;a<8;++a) {
      int dx=(a%2?10:0), dy=(a%2?0:10); (void)dx; (void)dy;
    }
    tft.fillRect(x+16,y+2,5,7,silver); tft.fillRect(x+16,y+27,5,7,silver);
    tft.fillRect(x+2,y+16,7,5,silver); tft.fillRect(x+27,y+16,7,5,silver);
    tft.fillCircle(x+18,y+18,5,0x5ACB);
    tft.fillCircle(x+18,y+18,2,bg);
  } else if (kind == "Th") {
    // Small painter's palette: distinct Themes icon with no bitmap/heap.
    tft.fillCircle(x+18,y+18,13,0xE71C);
    tft.drawCircle(x+18,y+18,13,0x4208);
    tft.fillCircle(x+12,y+11,3,0xF800);
    tft.fillCircle(x+20,y+9,3,0x04FF);
    tft.fillCircle(x+27,y+16,3,0x07E0);
    tft.fillCircle(x+13,y+24,3,0xFFE0);
    tft.fillCircle(x+23,y+24,4,bg);
  } else if (kind == "Note") {
    tft.fillRect(x+7,y+5,23,27,shadow);
    tft.fillRect(x+5,y+3,23,27,white);
    for(int i=0;i<5;++i) tft.drawFastHLine(x+9,y+10+i*4,14,0x5D9F);
    for(int i=0;i<5;++i) tft.fillRect(x+7+i*4,y+1,2,6,silver);
  } else if (kind == "App" || kind == "All") {
    tft.fillRect(x+4,y+10,27,20,0xFD20);
    tft.fillRect(x+7,y+7,12,6,0xFFE0);
    tft.drawRect(x+4,y+10,27,20,0x9A60);
    tft.fillRect(x+19,y+18,6,6,0x04FF);
    tft.fillRect(x+25,y+14,6,6,0x07E0);
    tft.fillRect(x+27,y+22,6,6,0xF800);
  } else if (kind == "Doc") {
    tft.fillRect(x+9,y+5,23,27,shadow);
    tft.fillRect(x+5,y+3,23,27,white);
    tft.drawRect(x+5,y+3,23,27,silver);
    for(int k=0;k<4;++k) tft.drawFastHLine(x+9,y+10+k*4,14,0x5D9F);
  } else if (kind == "Bell") {
    tft.fillCircle(x+18,y+13,7,0xFDE0);
    tft.fillRect(x+11,y+13,14,10,0xFDE0);
    tft.drawFastHLine(x+8,y+24,20,0xFDE0);
    tft.fillCircle(x+18,y+27,3,white);
  } else if (kind == "Lock") {
    tft.drawCircle(x+18,y+13,7,silver);
    tft.fillRect(x+9,y+14,18,16,0x4208);
    tft.drawRect(x+9,y+14,18,16,silver);
    tft.fillCircle(x+18,y+21,2,white);
  } else if (kind == "Br") {
    tft.fillCircle(x+18,y+18,11,0xFFE0);
    tft.drawFastHLine(x+16,y+2,4,0xFFE0);
    tft.drawFastVLine(x+16,y+31,4,0xFFE0);
    tft.drawFastHLine(x+1,y+16,4,0xFFE0);
    tft.drawFastHLine(x+31,y+16,4,0xFFE0);
  } else if (kind == "i") {
    tft.fillCircle(x+18,y+18,13,0x001F);
    tft.drawCircle(x+18,y+18,13,white);
    tft.fillCircle(x+18,y+11,2,white);
    tft.fillRect(x+17,y+16,3,11,white);
  } else if (kind == "Quick") {
    tft.fillRect(x+5,y+7,26,4,0x04FF);
    tft.fillRect(x+5,y+16,26,4,0x07E0);
    tft.fillRect(x+5,y+25,26,4,0xFFE0);
    tft.fillCircle(x+13,y+9,3,white);tft.fillCircle(x+24,y+18,3,white);
    tft.fillCircle(x+15,y+27,3,white);
  } else if (kind == "Clk") {
    tft.fillCircle(x+18,y+18,13,white);
    tft.drawCircle(x+18,y+18,13,0x5ACB);
    tft.drawFastVLine(x+18,y+8,11,black);
    tft.drawLine(x+18,y+18,x+25,y+22,black);
    tft.fillCircle(x+18,y+18,2,0xF800);
  } else {
    // Generic S60 tile for secondary apps.
    tft.fillRect(x+4,y+4,28,28,0x04FF);
    tft.drawRect(x+4,y+4,28,28,white);
    String s=kind; if(s.length()>2)s=s.substring(0,2);
    tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(white,0x04FF);
    int tw=tft.textWidth(s); tft.setCursor(x+(ICON_BOX-tw)/2,y+10); tft.print(s); tft.setTextFont(1);
  }
}

void SymbianUI::drawIcon(int x, int y, const String &kind, uint16_t color) {
  drawS60MenuIcon(x, y, kind, color);
}

void SymbianUI::listItem(int row, const String &icon, const String &title, const String &sub, bool selected) {
  if (row < 0 || row >= LIST_VISIBLE) return;
  const int y = CONTENT_TOP + 1 + row * LIST_ROW_H;
  const uint16_t bg = selected ? colors.selected : colors.bg;
  tft.fillRect(2, y, Board::SCREEN_W - 7, LIST_ROW_H - 1, bg);
  if (selected) {
    tft.drawRect(2, y, Board::SCREEN_W - 7, LIST_ROW_H - 1, colors.border);
    tft.drawFastVLine(3, y + 1, LIST_ROW_H - 3, colors.chrome);
  }

  // All twelve core glyphs use the hand-rastered 24x24 variant in lists;
  // extra v2.3 glyphs keep their existing, source-compatible 36px fallback.
  const VqeafIcons::Id standardId=VqeafIcons::fromLegacy(UiIconCatalog::canonical(icon.c_str()));
  if (standardId != VqeafIcons::Id::Count)
    VqeafIcons::draw(tft, standardId, 12, y + 9, 24, bg, VqeafIcons::Palette::standard());
  else drawIcon(7, y + 3, icon, bg);

  const uint16_t labelInk = selected ? selectedInk : colors.text;
  tft.setTextColor(labelInk, bg);
  tft.setTextFont(UiTypography::BODY);
  tft.setTextSize(1);
  const int textX = 48;
  String ttl = title;
  ttl = fitTextPixels(ttl, UiTypography::BODY, Board::SCREEN_W - textX - 8);
  textBold(textX, y + 3, ttl, UiTypography::BODY, labelInk, bg);

  if (sub.length()) {
    tft.setTextFont(1);
    tft.setTextColor(colors.dim, bg);
    tft.setCursor(textX + 1, y + 24);
    String line = sub;
    line = fitTextPixels(line, UiTypography::MICRO, Board::SCREEN_W - textX - 8);
    if (UiVietnameseFont::hasUtf8(line.c_str()))
      UiVietnameseFont::draw(tft,textX+1,y+24,line.c_str(),colors.dim,bg,0,Board::SCREEN_W-textX-9);
    else tft.print(line);
  }
  tft.setTextFont(1);
}

void SymbianUI::gridItem(int slot, const String &icon, const String &title, bool selected) {
  if (slot < 0 || slot >= GRID_COLS * GRID_ROWS) return;
  const int col = slot % GRID_COLS;
  const int row = slot / GRID_COLS;
  const int x = 1 + col * GRID_CELL_W;
  const int y = CONTENT_TOP + row * GRID_CELL_H;
  const int w = GRID_CELL_W - 2;
  const int h = GRID_CELL_H;
  const uint16_t base = (themeId == ThemeId::S60Green || themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? menuRowColor(row) : colors.bg;
  const uint16_t bg = selected ? colors.selected : base;

  tft.fillRect(x, y, w, h, bg);
  // Restore background row seam after a focus-only cell refresh.
  if (row > 0 && !selected && themeId == ThemeId::S60Green)
    tft.drawFastHLine(x, y, w, 0xAEE9);
  if (selected) {
    if (themeId == ThemeId::S60Green) {
      VqeafIcons::focusFrame(tft,x+1,y+1,w-2,h-2,TFT_WHITE,0xB6EE);
      // Original reference frame: discrete white glints at the top corners.
      tft.fillRect(x+3,y+3,17,2,0xF7FC);
      tft.fillRect(x+w-20,y+3,17,2,0xF7FC);
    } else if (themeId == ThemeId::AmoledRed || themeId == ThemeId::External) {
      VqeafIcons::focusFrame(tft,x+1,y+1,w-2,h-2,colors.accent,colors.border);
      tft.fillRect(x+3,y+3,w-6,2,colors.accent);
    } else {
      tft.drawRect(x, y, w, h, colors.border);
      tft.drawRect(x + 1, y + 1, w - 2, h - 2, 0x8410);
    }
  }

  const int iconX = x + (w - ICON_BOX) / 2;
  const int iconY = y + 3;
  // VQEAF v2.3.4 pixel-art RGB565 icons (36px) for 12 system slots. Aliases
  // remain accepted for non-system/custom grid items, so old apps are safe.
  const VqeafIcons::Id expected = UiIconCatalog::menuAsset(slot);
  const VqeafIcons::Id requested = VqeafIcons::fromLegacy(
      UiIconCatalog::canonical(icon.c_str()));
  if (expected != VqeafIcons::Id::Count && expected == requested) {
    if (!VqeafIcons::draw(tft, expected, iconX, iconY, 36, bg,
                          VqeafIcons::Palette::standard()))
      drawIcon(iconX, iconY, icon, bg);
  } else {
    drawIcon(iconX, iconY, icon, bg);
  }

  String label = title;
  label = fitTextPixels(label, UiTypography::CAPTION, w - 6);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(selected ? selectedInk : colors.text, bg);
  int tw = textWidth(label, UiTypography::CAPTION);
  textBold(x + max(2, (w - tw) / 2), y + UiTypography::MENU_CAPTION_Y, label, UiTypography::CAPTION, selected ? selectedInk : colors.text, bg);
}

void SymbianUI::gridHint(const String &title, const String &sub) {
  const int y = SOFTKEY_TOP - 24;
  const int h = 22;
  tft.fillRect(3, y, Board::SCREEN_W - 8, h, colors.panel);
  tft.drawFastHLine(3, y, Board::SCREEN_W - 8, colors.dim);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(colors.text, colors.panel);
  String line = title + "  " + sub;
  if (line.length() > 36) line = line.substring(0,35) + "~";
  tft.setCursor(7, y + 7); tft.print(line);
}

void SymbianUI::scrollbar(int total, int visible, int offset, int topY, int bottomY) {
  const int x = Board::SCREEN_W - 5;
  const int h = max(8, bottomY - topY);
  // Always erase the old track first so shrinking lists never leave a stale thumb.
  tft.fillRect(x, topY, 3, h, colors.bg);
  if (total <= visible || total <= 0) return;
  tft.fillRect(x, topY, 3, h, 0x5ACB);

  int thumbH = max(12, (h * visible) / total);
  int maxOffset = max(1, total - visible);
  int travel = max(0, h - thumbH);
  int thumbY = topY + (travel * constrain(offset, 0, maxOffset)) / maxOffset;
  tft.fillRect(x, thumbY, 3, thumbH, colors.chromeText);
}

void SymbianUI::popupMenu(const char *const items[], int count, int selected, int offset, int visible) {
  if (count <= 0) return;
  visible = min(visible, count);
  offset = constrain(offset, 0, max(0, count - visible));

  const int rowH = 31;
  const int w = 214;
  const int h = visible * rowH + 6;
  const int x = 5;
  const int y = SOFTKEY_TOP - h - 2;

  tft.fillRect(x + 4, y + 4, w, h, 0x0000);
  tft.fillRect(x, y, w, h, colors.popup);
  tft.drawRect(x, y, w, h, colors.border);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, 0x8C51);

  tft.setTextFont(2);
  tft.setTextSize(1);
  for (int row = 0; row < visible; ++row) {
    int i = offset + row;
    if (i >= count) break;
    int iy = y + 3 + row * rowH;
    uint16_t bg = (i == selected) ? colors.popupSelected : colors.popup;
    uint16_t fg = (i == selected) ? (themeId == ThemeId::External ? selectedInk : TFT_WHITE) : colors.popupText;
    tft.fillRect(x + 3, iy, w - 13, rowH - 1, bg);
    if (i == selected) tft.drawRect(x + 3, iy, w - 13, rowH - 1, TFT_WHITE);
    tft.setTextColor(fg, bg);
    textBold(x + 10, iy + 6, String(items[i]), 2, fg, bg);
  }
  tft.setTextFont(1);

  if (count > visible) {
    int trackX = x + w - 7;
    int trackY = y + 5;
    int trackH = h - 10;
    tft.fillRect(trackX, trackY, 3, trackH, 0xBDF7);
    int thumbH = max(10, (trackH * visible) / count);
    int maxOffset = max(1, count - visible);
    int thumbY = trackY + ((trackH - thumbH) * offset) / maxOffset;
    tft.fillRect(trackX, thumbY, 3, thumbH, 0x2104);
  }
}

void SymbianUI::message(const String &title, const String &line1, const String &line2, const String &line3) {
  tft.fillRect(0, CONTENT_TOP, Board::SCREEN_W, SOFTKEY_TOP - CONTENT_TOP, colors.bg);
  tft.setTextColor(colors.text, colors.bg);
  tft.setTextFont(2);
  tft.setTextSize(1);
  textBold(12, 50, title, 2, colors.text, colors.bg, true);
  tft.setTextFont(1);
  tft.setCursor(12, 86); tft.print(line1);
  if (line2.length()) { tft.setCursor(12, 106); tft.print(line2); }
  if (line3.length()) { tft.setCursor(12, 126); tft.print(line3); }
}


void SymbianUI::drawWallpaper() {
  if (themeId == ThemeId::External) {
    // Simple optional banding: no Retro-Go asset copied, no extra framebuffer.
    tft.fillScreen(colors.bg);
    for (int i=0; i<4; ++i)
      tft.fillRect(0, i*80, Board::SCREEN_W, 80, themeMix565(colors.bg, colors.panel, i));
    return;
  }
  if (themeId == ThemeId::AmoledRed) {
    tft.fillScreen(TFT_BLACK);
    // AMOLED-friendly near-black wallpaper with sparse red glow lines.
    for (int i=0;i<7;++i) {
      int y=i*44;
      uint16_t c=(i&1)?0x0801:0x0000;
      tft.fillRect(0,y,Board::SCREEN_W,44,c);
    }
    for (int i=0;i<5;++i) {
      tft.drawLine(0,250+i*4,Board::SCREEN_W,135+i*8,0x2801);
      tft.drawLine(40,319-i*3,230,220+i*2,colors.border);
    }
    return;
  }
  if (themeId == ThemeId::S60Green) {
    // Active-standby wallpaper reuses the same S60 palette without storing a bitmap.
    static const uint16_t bands[] = {0x4BE5,0x6542,0x75A3,0x8E25,0x96A6,0x8E44,0xA6C8};
    const int bh = 46;
    for (int i=0;i<7;++i) tft.fillRect(0, i*bh, Board::SCREEN_W, bh, bands[i]);
    // Low-cost leaf/swoosh accents; direct lines cost flash only and no heap.
    for (int i=0;i<5;++i) {
      tft.drawLine(0, 260+i*3, 150, 170+i*5, 0xB6EE);
      tft.drawLine(45, 319-i*4, 235, 215+i*2, 0x75A3);
    }
    return;
  }
  static const uint16_t bands[] = {0x0010,0x0015,0x0218,0x031A,0x041C,0x051E};
  const int bandH = 44;
  for (int i=0;i<6;++i) tft.fillRect(0, i*bandH, Board::SCREEN_W, bandH, bands[i]);
  tft.fillRect(0, 245, Board::SCREEN_W, 47, 0x1082);
  for (int x=0; x<Board::SCREEN_W; x+=18) {
    int h = 10 + ((x/18)%4)*5;
    tft.fillRect(x, 245-h, 13, h, 0x18C3);
  }
}

void SymbianUI::idleHome(bool wifi, bool ble, bool sd, bool hour12, int shortcutIndex, int unreadNotifications, bool musicPlaying, const String &wifiLine) {
  // Standby is intentionally drawn once. Clock and focus updates use small dirty
  // rectangles so a one-second timer never repaints the full LCD.
  drawWallpaper();
  invalidateChrome();

  // S60 standby status strip uses the same 80/80/80 layout as app chrome.
  tft.fillRect(0, 0, Board::SCREEN_W, TITLEBAR_H, colors.chrome);
  if (themeId == ThemeId::S60Green) {
    tft.drawFastHLine(0, TITLEBAR_H - 2, Board::SCREEN_W, 0x4BE5);
    tft.drawFastHLine(0, TITLEBAR_H - 1, Board::SCREEN_W, 0xAEE9);
  } else if (themeId == ThemeId::AmoledRed || themeId == ThemeId::External) {
    tft.drawFastHLine(0, TITLEBAR_H - 2, Board::SCREEN_W, colors.border);
    tft.drawFastHLine(0, TITLEBAR_H - 1, Board::SCREEN_W, colors.accent);
  } else {
    tft.drawFastHLine(0, TITLEBAR_H - 2, Board::SCREEN_W, 0x8410);
    tft.drawFastHLine(0, TITLEBAR_H - 1, Board::SCREEN_W, 0x8410);
  }
  tft.setTextColor(colors.chromeText, colors.chrome);
  tft.setTextFont(2);
  tft.setTextSize(1);
  textBold(UiTypography::HEADER_TITLE_X,UiTypography::HEADER_TITLE_Y,"General",
           UiTypography::TITLE,colors.chromeText,colors.chrome,true);

  String topTime = timeText(hour12);
  tft.setTextFont(1);
  int topTw = tft.textWidth(topTime);
  tft.setCursor((Board::SCREEN_W - topTw) / 2, 9); tft.print(topTime);

  (void)ble; (void)sd;
  const int batteryX = Board::SCREEN_W - STATUS_RIGHT_PAD - STATUS_ICON_W;
  const int wifiX = batteryX - STATUS_ICON_GAP - STATUS_ICON_W;
  if (wifi) drawStatusWifi(wifiX, 8, colors.chromeText);
  drawStatusBattery(batteryX, 7, colors.chromeText);

  // Notification badge stays inside the title zone so it cannot disturb the
  // centered clock or the equally-sized right-hand status slots.
  if (unreadNotifications > 0) {
    tft.fillCircle(68, 12, 6, colors.chromeText);
    tft.setTextFont(1);
    tft.setTextColor(colors.chrome, colors.chromeText);
    tft.setCursor(66, 9); tft.print(min(unreadNotifications, 9));
    tft.setTextColor(colors.chromeText, colors.chrome);
  }

  // Clock card has a stable background so only this small area is touched each second.
  const uint16_t standbyPanel = themeId == ThemeId::S60Green ? 0xDF93 : ((themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? colors.panel : 0x1082);
  tft.fillRect(12, 43, Board::SCREEN_W - 24, 78, standbyPanel);
  tft.drawRect(12, 43, Board::SCREEN_W - 24, 78, themeId == ThemeId::S60Green ? TFT_WHITE : (themeId == ThemeId::AmoledRed ? colors.border : 0x39E7));
  idleClock(hour12);

  // Compact device status, deliberately one-line-per-service like S60 Active Standby.
  const uint16_t statusPanel = themeId == ThemeId::S60Green ? 0xB6EE : ((themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? colors.panel : 0x18C3);
  tft.fillRect(12, 132, Board::SCREEN_W - 24, 46, statusPanel);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(themeId == ThemeId::S60Green ? TFT_BLACK : ((themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? colors.text : 0xE71C), statusPanel);
  tft.setCursor(18, 140);
  String firstLine = wifiLine.length() ? wifiLine :
      ((wifi && WiFi.SSID().length()) ? String("WiFi  ") + WiFi.SSID() : "WiFi  offline");
  firstLine = fitTextPixels(firstLine, UiTypography::MICRO, 204);
  if (UiVietnameseFont::hasUtf8(firstLine.c_str()))
    UiVietnameseFont::draw(tft,18,140,firstLine.c_str(),
      themeId==ThemeId::S60Green?TFT_BLACK:colors.text,statusPanel,0,204);
  else tft.print(firstLine);
  tft.setCursor(18, 154);
  if (musicPlaying) tft.print("Music playing");
  else if (unreadNotifications) tft.print(String(unreadNotifications) + " unread notification(s)");
  else tft.print("Device ready");

  idleShortcuts(shortcutIndex);
  softkeys("Menu", "Open", "Quick");
}


void SymbianUI::idleNetworkStatus(const String &line, bool connected) {
  // Only the status line and its compact WiFi/battery glyph group are dirty.
  // Never redraw the wallpaper, launcher, clock card or softkeys for RF events.
  const uint16_t panel = themeId == ThemeId::S60Green ? 0xB6EE :
                         ((themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? colors.panel : 0x18C3);
  const uint16_t ink = themeId == ThemeId::S60Green ? TFT_BLACK :
                       ((themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? colors.text : 0xE71C);
  tft.fillRect(18, 137, 205, 15, panel);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(ink, panel);
  String clipped = line;
  clipped = fitTextPixels(clipped, UiTypography::MICRO, 204);
  tft.setCursor(18, 140);
  if (UiVietnameseFont::hasUtf8(clipped.c_str()))
    UiVietnameseFont::draw(tft,18,140,clipped.c_str(),ink,panel,0,204);
  else tft.print(clipped);

  const int batteryX = Board::SCREEN_W - STATUS_RIGHT_PAD - STATUS_ICON_W;
  const int wifiX = batteryX - STATUS_ICON_GAP - STATUS_ICON_W;
  // 45px at far right, never overlaps the clock centered at x=120.
  tft.fillRect(195, 0, Board::SCREEN_W - 195, TITLEBAR_H - 2, colors.chrome);
  const uint8_t bars = connected ? statusWifiBars() : 0;
  if (bars) drawStatusWifi(wifiX, 8, colors.chromeText);
  drawStatusBattery(batteryX, 7, colors.chromeText);
}


void SymbianUI::idleClock(bool hour12) {
  const uint16_t panel = themeId == ThemeId::S60Green ? 0xDF93 : ((themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? colors.panel : 0x1082);
  tft.fillRect(18, 50, Board::SCREEN_W - 36, 63, panel);
  String tm = timeText(hour12);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextColor(themeId == ThemeId::S60Green ? TFT_BLACK : colors.text, panel);
  int tw = tft.textWidth(tm);
  tft.setCursor((Board::SCREEN_W - tw) / 2, 53);
  tft.print(tm);
  tft.setTextSize(1);
  String dt = dateText();
  tft.setTextFont(2);
  int dw = tft.textWidth(dt);
  tft.setCursor((Board::SCREEN_W - dw) / 2, 91);
  tft.print(dt);
  tft.setTextFont(1);
}

void SymbianUI::idleShortcutTile(int i, bool selected) {
  static const char *labels[] = {"WiFi", "Music", "Files"};
  if (i < 0 || i >= 3) return;
  const VqeafLayout::Rect r=VqeafLayout::homeShortcut(i);
  const uint16_t bg=selected ? colors.selected : colors.panel;
  tft.fillRect(r.x,r.y,r.w,r.h,bg);
  tft.drawRect(r.x,r.y,r.w,r.h,selected?colors.border:colors.dim);
  if (selected) {
    tft.drawFastHLine(r.x+2,r.y+2,17, colors.chrome);
    tft.drawFastHLine(r.x+r.w-19,r.y+2,17,colors.chrome);
  }
  // Home shares the exact 36x36 pixel-art bitmaps used in Menu (no runtime resize).
  // The shortcuts are centered using the same pixel contract as the atlas.
  const VqeafIcons::Id id = UiIconCatalog::homeAsset(i);
  const int iconX = r.x + (r.w - ICON_BOX) / 2;
  if (!VqeafIcons::draw(tft, id, iconX, r.y + 6, 36, bg,
                        VqeafIcons::Palette::standard()))
    drawIcon(iconX, r.y + 6, labels[i], bg);
  tft.setTextSize(1);
  const uint16_t fg=selected?selectedInk:colors.text;
  const int w=textWidth(labels[i],UiTypography::CAPTION);
  textBold(r.x+(r.w-w)/2,r.y+47,labels[i],UiTypography::CAPTION,fg,bg);
}
void SymbianUI::idleShortcuts(int shortcutIndex) {
  for(int i=0;i<3;++i)idleShortcutTile(i,i==shortcutIndex);
  const VqeafLayout::Rect h=VqeafLayout::HOME_HINT;
  const uint16_t hintBg=themeId==ThemeId::S60Green?0x4BE5:
    (themeId==ThemeId::External?colors.chrome:0x1082);
  tft.fillRect(h.x,h.y,h.w,h.h,hintBg);
  tft.setTextFont(UiTypography::MICRO);
  tft.setTextColor(themeId==ThemeId::S60Green?TFT_WHITE:colors.chromeText,hintBg);
  tft.setCursor(h.x+5,UiTypography::HOME_HINT_Y);
  tft.print("Hold MENU: tasks  OPT: settings");
}
void SymbianUI::idleShortcutDelta(int previous, int next) {
  if(previous==next)return;
  idleShortcutTile(previous,false);
  idleShortcutTile(next,true);
}

void SymbianUI::lockScreen(bool wifi, bool ble, bool sd, bool hour12, int unreadNotifications, bool dimmed) {
  drawWallpaper();
  invalidateChrome();
  const uint16_t panel = dimmed ? 0x0000 : (themeId == ThemeId::S60Green ? 0x4BE5 : 0x1082);
  tft.fillRect(14, 39, Board::SCREEN_W - 28, 205, panel);
  tft.drawRect(14, 39, Board::SCREEN_W - 28, 205, dimmed ? 0x4208 : 0xBDF7);

  lockClock(hour12, dimmed);
  drawIcon(106, 146, "Lock", panel);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(dimmed ? 0x7BEF : colors.text, panel);
  const char *label = "Keypad locked";
  const int lw = tft.textWidth(label);
  tft.setCursor((Board::SCREEN_W - lw) / 2, 183); tft.print(label);
  tft.setTextFont(1);
  String n = unreadNotifications ? String(unreadNotifications) + " notification(s)" : "No notifications";
  int nw = tft.textWidth(n);
  tft.setCursor((Board::SCREEN_W - nw) / 2, 210); tft.print(n);

  // No SIM/modem on this device: lock status is WiFi + battery only.
  (void)ble; (void)sd;
  if (wifi) drawStatusWifi(98, 231, dimmed ? 0x7BEF : TFT_WHITE);
  drawStatusBattery(126, 230, dimmed ? 0x7BEF : TFT_WHITE);

  const uint16_t lockFooter = themeId == ThemeId::S60Green ? 0x2B63 : 0x1082;
  tft.fillRect(0, 260, Board::SCREEN_W, 36, lockFooter);
  tft.setTextColor(themeId == ThemeId::S60Green ? TFT_WHITE : 0xC618, lockFooter);
  tft.setCursor(42, 273); tft.print("Press START to unlock");
  softkeys("", "Unlock", "");
}


void SymbianUI::lockClock(bool hour12, bool dimmed) {
  const uint16_t panel = dimmed ? 0x0000 : (themeId == ThemeId::S60Green ? 0x4BE5 : 0x1082);
  tft.fillRect(22, 54, Board::SCREEN_W - 44, 78, panel);
  String tm = timeText(hour12);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextColor(dimmed ? 0x7BEF : TFT_WHITE, panel);
  int tw = tft.textWidth(tm);
  tft.setCursor((Board::SCREEN_W - tw) / 2, 59); tft.print(tm);
  tft.setTextSize(1);
  String dt = dateText();
  tft.setTextFont(2);
  int dw = tft.textWidth(dt);
  tft.setCursor((Board::SCREEN_W - dw) / 2, 99); tft.print(dt);
  tft.setTextFont(1);
}

void SymbianUI::clockFace(bool hour12, bool full) {
  if (full) {
    clearContent();
    tft.fillRect(12, 64, Board::SCREEN_W - 24, 113, colors.panel);
    tft.drawRect(12, 64, Board::SCREEN_W - 24, 113, colors.dim);
    tft.setTextFont(1);
    tft.setTextColor(colors.dim, colors.bg);
    tft.setCursor(26, 205);
    tft.print("Network time syncs automatically over WiFi");
  }
  tft.fillRect(20, 75, Board::SCREEN_W - 40, 88, colors.panel);
  String tm = timeText(hour12);
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextColor(colors.text, colors.panel);
  int tw = tft.textWidth(tm);
  tft.setCursor((Board::SCREEN_W - tw) / 2, 81); tft.print(tm);
  tft.setTextSize(1);
  String dt = dateText();
  tft.setTextFont(2);
  int dw = tft.textWidth(dt);
  tft.setCursor((Board::SCREEN_W - dw) / 2, 126); tft.print(dt);
  tft.setTextFont(1);
}

String SymbianUI::dateText() {
  struct tm info;
  if (getLocalTime(&info, 5)) {
    char b[24];
    strftime(b, sizeof(b), "%a %d %b", &info);
    return String(b);
  }
  return String("Date not set");
}

void SymbianUI::dialog(const String &title, const String &line1, const String &line2,
                       const String &left, const String &right, int selected) {
  const int x=14, y=88, w=Board::SCREEN_W-28, h=132;
  tft.fillRect(x+4,y+4,w,h,0x0000);
  tft.fillRect(x,y,w,h,colors.popup);
  tft.drawRect(x,y,w,h,colors.border);
  tft.drawRect(x+1,y+1,w-2,h-2,0x8C51);
  tft.fillRect(x+3,y+3,w-6,27,colors.chrome);
  tft.setTextFont(2); tft.setTextColor(colors.chromeText,colors.chrome);
  textBold(x+9,y+7,title,2,colors.chromeText,colors.chrome,true);
  tft.setTextFont(1); tft.setTextColor(colors.popupText,colors.popup);
  tft.setCursor(x+10,y+43); tft.print(line1);
  if (line2.length()) { tft.setCursor(x+10,y+60); tft.print(line2); }
  const int by=y+h-33, bw=(w-26)/2;
  uint16_t lbg=selected==0?colors.popupSelected:colors.popup;
  uint16_t rbg=selected==1?colors.popupSelected:colors.popup;
  tft.fillRect(x+8,by,bw,24,lbg); tft.drawRect(x+8,by,bw,24,colors.border);
  tft.fillRect(x+18+bw,by,bw,24,rbg); tft.drawRect(x+18+bw,by,bw,24,colors.border);
  tft.setTextFont(1);
  tft.setTextColor(selected==0?(themeId==ThemeId::External?selectedInk:TFT_WHITE):colors.popupText,lbg);
  int lw=tft.textWidth(left); tft.setCursor(x+8+(bw-lw)/2,by+8); tft.print(left);
  tft.setTextColor(selected==1?(themeId==ThemeId::External?selectedInk:TFT_WHITE):colors.popupText,rbg);
  int rw=tft.textWidth(right); tft.setCursor(x+18+bw+(bw-rw)/2,by+8); tft.print(right);
}

void SymbianUI::transitionOut() {
  // Lightweight S60-style sweep. It never blanks the whole LCD, avoiding the
  // visible black flash of the old center-closing transition.
  const int top = CONTENT_TOP;
  const int h = SOFTKEY_TOP - CONTENT_TOP;
  for (int x = 0; x < Board::SCREEN_W; x += 48) {
    tft.fillRect(x, top, 2, h, colors.accent);
    delay(2);
    tft.fillRect(x, top, 2, h, colors.bg);
  }
}
void SymbianUI::openingApp(const String &name, const String &icon, bool resume) {
  // S60-style application launch interstitial. It is intentionally short and
  // uses only the content area so titlebar/softkey repaint remains controlled.
  clearContent();
  const int cx = Board::SCREEN_W / 2;
  drawIcon(cx - ICON_BOX/2, 102, icon, colors.bg);
  tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(colors.text, colors.bg);
  String action = resume ? "Dang tiep tuc" : "Dang mo ung dung";
  int aw = tft.textWidth(action); textBold((Board::SCREEN_W-aw)/2, 145, action, 2, colors.text, colors.bg, true);
  tft.setTextFont(1); tft.setTextColor(colors.dim, colors.bg);
  String n = name; if (n.length() > 26) n = n.substring(0,25) + "~";
  int nw=tft.textWidth(n); tft.setCursor((Board::SCREEN_W-nw)/2, 169); tft.print(n);
  tft.drawRect(54, 198, 132, 7, colors.dim);
  tft.fillRect(56, 200, 92, 3, colors.accent);
  softkeys("", "", "");
}

void SymbianUI::progress(int x, int y, int w, int pct) {
  pct = constrain(pct, 0, 100);
  tft.drawRect(x, y, w, 10, colors.dim);
  tft.fillRect(x + 2, y + 2, (w - 4) * pct / 100, 6, colors.accent);
}
