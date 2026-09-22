#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <esp_sleep.h>
#include "src/lua_vm.h"

// TFT pins
#define TFT_LED 39
#define TFT_DC 47
#define TFT_CS 14
#define TFT_SCL 48
#define TFT_SDA 12
#define TFT_RST 3

// SD card pins (SPI mode)
#define SD_CS_PIN 10
#define SD_MOSI 11
#define SD_SCLK 13
#define SD_MISO 9

// Buttons
#define KEY_UP 7
#define KEY_DOWN 46
#define KEY_LEFT 45
#define KEY_RIGHT 6
#define KEY_MENU 18
#define KEY_OPTION 8
#define KEY_START 16
#define KEY_START 17
#define KEY_A 15
#define KEY_B 5
#define KEY_BOOT 0

#define HOLD_RESTART_MS 3000
#define HOLD_SLEEP_MS 3000

#define CMD_CLEAR 1
#define CMD_PIXEL 2
#define CMD_LINE 3
#define CMD_RECT 4
#define CMD_FILL 5
#define CMD_TEXT 6
#define CMD_COLOR 7
#define CMD_DELAY 8
#define CMD_CIRCLE 9

#define MAX_ENTRIES 128
#define TEXT_LINES 8
#define MAX_TEXT_LINE_LENGTH 32

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
SPIClass sdSPI(HSPI);

enum FileType {
  TYPE_UNKNOWN,
  TYPE_TXT,
  TYPE_LUA,
  TYPE_PY,
  TYPE_HTM,
  TYPE_JSON,
  TYPE_SCR,
  TYPE_IMG,
  TYPE_ROM,
  TYPE_DIR
};

struct FileEntry {
  String name;
  String path;
  bool isDir;
  uint32_t size;
  FileType type;
};

FileEntry entries[MAX_ENTRIES];
int entryCount = 0;
int currentIndex = 0;
int listOffset = 0;
int prevIndex = -1;
String currentFolder = "/";
String editBuffer = "";
bool editMode = false;

String copySource = "";
bool cutMode = false;

uint16_t luaTextColor = ST77XX_WHITE;

int luaTextSize = 1;

unsigned long keyAPressTime = 0;
bool keyAShowingMsg = false;
bool isWakeFromSleep = false;
unsigned long keyBPressTime = 0;
bool keyBShowingMsg = false;

struct VarValue { bool isString; String str; float num; };
struct LuaVar { String name; VarValue value; };
LuaVar luaVars[20];
int luaVarCount = 0;

void luaSetVar(const String &name, const VarValue &value);
bool luaHasVar(const String &name);
VarValue luaGetVar(const String &name);
VarValue evalExpression(const String &expr);

uint16_t getColorByName(const String &name);
String nextLuaArg(String &args);
void luaScreenClear();
void luaScreenPrint(int x, int y, const String &text, uint16_t color, int size);
void luaScreenDrawRect(int x, int y, int w, int h, const String &colorName, bool fill);

#define RGB(r,g,b) (((r&0xF8)<<8)|((g&0xFC)<<3)|(b&0xF8))

#define XP_TITLE_BG RGB(0,84,166)
#define XP_TITLE_BG_END RGB(10,36,114)
#define XP_WINDOW_BG RGB(236,233,216)
#define XP_WINDOW_BORDER RGB(0,0,128)
#define XP_BTN_FACE RGB(216,216,216)
#define XP_BTN_HIGHLIGHT RGB(255,255,255)
#define XP_BTN_SHADOW RGB(128,128,128)
#define XP_BTN_DARKSHADOW RGB(64,64,64)
#define XP_SELECT_BG RGB(10,36,114)
#define XP_SELECT_FG RGB(255,255,255)
#define XP_GRAY_TEXT RGB(128,128,128)
#define XP_BLACK RGB(0,0,0)
#define XP_WHITE RGB(255,255,255)
#define XP_BLUE RGB(0,0,128)
#define XP_DESKTOP RGB(0,128,128)

#define WIN_MARGIN 3
#define WIN_TITLE_H 20
#define WIN_BORDER 2

bool buttonState(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

void waitForButtonRelease(uint8_t pin) {
  while (digitalRead(pin) == LOW) {
    delay(10);
  }
  delay(80);
}

void drawXPTitleBar(const String &title, int y) {
  tft.fillRect(0, y, 240, WIN_TITLE_H, XP_TITLE_BG);
  for (int i = 0; i < WIN_TITLE_H; i++) {
    float ratio = (float)i / WIN_TITLE_H;
    uint8_t r = 0 + (uint8_t)((10 - 0) * ratio);
    uint8_t g = (uint8_t)(84 + (36 - 84) * ratio);
    uint8_t b = (uint8_t)(166 + (114 - 166) * ratio);
    tft.drawFastHLine(0, y + i, 240, RGB(r, g, b));
  }
  tft.fillRect(0, y + WIN_TITLE_H, 240, 1, XP_TITLE_BG_END);
  tft.fillRect(0, y + WIN_TITLE_H + 1, 240, 1, XP_BTN_HIGHLIGHT);
  tft.setCursor(4, y + 4);
  tft.setTextSize(1);
  tft.setTextColor(XP_WHITE);
  tft.print(title);
}

void drawXPWindow(const String &title, int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, XP_WINDOW_BG);
  tft.drawRect(x, y, w, h, XP_BTN_SHADOW);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, XP_WINDOW_BORDER);
  tft.fillRect(x + 2, y + 2, w - 4, h - 4, XP_WINDOW_BG);
  drawXPTitleBar(title, y + 2);
}

void drawXPButton(int x, int y, int w, int h, const String &text, bool pressed) {
  uint16_t bg = pressed ? XP_BTN_SHADOW : XP_BTN_FACE;
  tft.fillRect(x, y, w, h, bg);
  tft.drawRect(x, y, w, h, XP_BTN_HIGHLIGHT);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, XP_BTN_DARKSHADOW);
  tft.fillRect(x + 1, y + 1, w - 2, 1, XP_BTN_HIGHLIGHT);
  tft.fillRect(x + 1, y + 1, 1, h - 2, XP_BTN_HIGHLIGHT);
  tft.setTextSize(1);
  tft.setTextColor(XP_BLACK);
  int16_t x1, y1;
  uint16_t w1, h1;
  tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w1, &h1);
  tft.setCursor(x + (w - w1) / 2, y + (h - h1) / 2 + 1);
  tft.print(text);
}

void drawHeader() {
  tft.fillScreen(XP_DESKTOP);
  tft.fillRect(0, 0, 240, WIN_TITLE_H + WIN_MARGIN, XP_TITLE_BG);
  tft.setCursor(4, 4);
  tft.setTextSize(1);
  tft.setTextColor(XP_WHITE);
  tft.print("PochitaFM - ");
  tft.print(currentFolder);
}

void drawFooter(const String &text) {
  tft.fillRect(0, 304, 240, 16, RGB(216,231,252));
  tft.drawRect(0, 304, 240, 16, XP_BTN_SHADOW);
  tft.drawRect(1, 304, 238, 15, XP_BTN_HIGHLIGHT);
  tft.fillRect(1, 304, 238, 1, XP_BTN_HIGHLIGHT);
  tft.setCursor(4, 308);
  tft.setTextSize(1);
  tft.setTextColor(XP_BLACK);
  tft.print(text);
}

void drawMatrixBackground() {
  tft.fillScreen(XP_DESKTOP);
}

void drawBootStatusLine(int row, const String &label, bool ok, int x, int y) {
  tft.setCursor(x, y);
  tft.setTextSize(1);
  tft.setTextColor(XP_BLACK);
  tft.print(label);
  tft.setCursor(x + 100, y);
  tft.setTextColor(ok ? RGB(0,160,0) : RGB(200,0,0));
  tft.print(ok ? "OK" : "Failed");
}

bool showBootSplash() {
  tft.fillScreen(XP_DESKTOP);
  
  int winW = 200;
  int winH = 180;
  int winX = 20;
  int winY = 70;
  
  tft.fillRect(winX, winY, winW, winH, XP_WINDOW_BG);
  tft.drawRect(winX, winY, winW, winH, XP_BTN_SHADOW);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_WINDOW_BORDER);
  drawXPTitleBar("Pochita File Manager", winY + 2);
  
  tft.setTextSize(1);
  tft.setTextColor(XP_BLACK);
  tft.setCursor(winX + 12, winY + WIN_TITLE_H + 12);
  tft.print("Starting system...");

  bool cpuOk = true;
  bool boardOk = true;
  bool socOk = true;

  bool sdOk = SD.begin(SD_CS_PIN, sdSPI, 4000000U);
  bool storageOk = sdOk && SD.exists("/");

  const String lines[] = {"CPU", "Board", "SoC", "Memory (SD)", "Storage"};
  const bool results[] = {cpuOk, boardOk, socOk, sdOk, storageOk};
  const int lineCount = 5;

  for (int i = 0; i < lineCount; i++) {
    tft.setCursor(winX + 20, winY + WIN_TITLE_H + 28 + i * 18);
    tft.setTextSize(1);
    tft.setTextColor(XP_BLACK);
    tft.print(lines[i]);
    tft.print(" ... ");
    tft.setCursor(winX + 110, winY + WIN_TITLE_H + 28 + i * 18);
    tft.setTextColor(XP_GRAY_TEXT);
    tft.print("Checking");
    delay(300);

    tft.setCursor(winX + 110, winY + WIN_TITLE_H + 28 + i * 18);
    tft.setTextColor(results[i] ? RGB(0,160,0) : RGB(200,0,0));
    tft.print(results[i] ? "OK      " : "Failed  ");
    delay(200);
  }

  tft.setCursor(winX + 12, winY + winH - 40);
  tft.setTextSize(1);
  tft.setTextColor(RGB(0,100,0));
  tft.print("Lua interpreter ready!");

  tft.setCursor(winX + 12, winY + winH - 26);
  tft.setTextColor(XP_BLUE);
  tft.print("Press any key to continue...");

  delay(1500);
  return sdOk && storageOk;
}

String getFileTag(FileType type) {
  switch (type) {
    case TYPE_TXT: return "TXT";
    case TYPE_LUA: return "LUA";
    case TYPE_PY:  return "PY";
    case TYPE_HTM: return "HTM";
    case TYPE_JSON:return "JSON";
    case TYPE_SCR: return "SCR";
    case TYPE_IMG: return "IMG";
    case TYPE_ROM: return "ROM";
    case TYPE_DIR: return "DIR";
    default:       return "???";
  }
}

FileType detectFileType(const String &name, bool isDir) {
  if (isDir) return TYPE_DIR;
  String lower = name;
  lower.toLowerCase();
  if (lower.endsWith(".txt") || lower.endsWith(".log") || lower.endsWith(".ino") || lower.endsWith(".cpp") || lower.endsWith(".h")) return TYPE_TXT;
  if (lower.endsWith(".lua")) return TYPE_LUA;
  if (lower.endsWith(".py")) return TYPE_PY;
  if (lower.endsWith(".html") || lower.endsWith(".htm")) return TYPE_HTM;
  if (lower.endsWith(".json")) return TYPE_JSON;
  if (lower.endsWith(".js") || lower.endsWith(".php") || lower.endsWith(".css") || lower.endsWith(".gs")) return TYPE_SCR;
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".bmp") || lower.endsWith(".png")) return TYPE_IMG;
  if (lower.endsWith(".nes") || lower.endsWith(".gb") || lower.endsWith(".gbc") || lower.endsWith(".bin") || lower.endsWith(".img") || lower.endsWith(".smc") || lower.endsWith(".sfc") || lower.endsWith(".sms") || lower.endsWith(".sg1000") || lower.endsWith(".col") || lower.endsWith(".cv") || lower.endsWith(".md") || lower.endsWith(".gen") || lower.endsWith(".gg") || lower.endsWith(".pce") || lower.endsWith(".lynx") || lower.endsWith(".wad") || lower.endsWith(".pwad")) return TYPE_ROM;
  return TYPE_UNKNOWN;
}

int compareEntries(const void *a, const void *b) {
  const FileEntry *fa = (const FileEntry *)a;
  const FileEntry *fb = (const FileEntry *)b;
  if (fa->isDir != fb->isDir) return fb->isDir ? 1 : -1;
  String aName = fa->name;
  String bName = fb->name;
  aName.toLowerCase();
  bName.toLowerCase();
  return aName.compareTo(bName);
}

String trimPath(const String &fullPath) {
  int idx = fullPath.lastIndexOf('/');
  if (idx < 0) return fullPath;
  return fullPath.substring(idx + 1);
}

String normalizePath(const String &path) {
  if (path.length() == 0) return "/";
  String result = path;
  if (!result.startsWith("/")) result = "/" + result;
  while (result.endsWith("/") && result.length() > 1) {
    result.remove(result.length() - 1);
  }
  return result;
}

String joinPath(const String &base, const String &name) {
  String b = normalizePath(base);
  String n = name;
  while (n.startsWith("/")) n = n.substring(1);
  if (b == "/") return String("/") + n;
  return b + "/" + n;
}

uint16_t getColorByName(const String &name) {
  String lower = name;
  lower.toLowerCase();
  if (lower == "black") return ST77XX_BLACK;
  if (lower == "white") return ST77XX_WHITE;
  if (lower == "red") return ST77XX_RED;
  if (lower == "green") return ST77XX_GREEN;
  if (lower == "blue") return ST77XX_BLUE;
  if (lower == "yellow") return ST77XX_YELLOW;
  if (lower == "cyan") return ST77XX_CYAN;
  if (lower == "magenta") return ST77XX_MAGENTA;
  if (lower == "orange") return ST77XX_ORANGE;
  if (lower == "purple") return ST77XX_MAGENTA; // Use magenta as purple alternative
  return ST77XX_WHITE;
}

String nextLuaArg(String &args) {
  args.trim();
  if (args.length() == 0) return "";
  bool inString = false;
  int split = args.length();
  for (int i = 0; i < args.length(); i++) {
    char c = args[i];
    if (c == '"') inString = !inString;
    if (c == ',' && !inString) {
      split = i;
      break;
    }
  }
  String result = args.substring(0, split);
  args = (split < args.length()) ? args.substring(split + 1) : "";
  result.trim();
  return result;
}

void luaScreenClear() {
  tft.fillScreen(ST77XX_BLACK);
}

void luaScreenPrint(int x, int y, const String &text, uint16_t color, int size) {
  tft.setCursor(x, y);
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.print(text);
}

void luaScreenDrawRect(int x, int y, int w, int h, const String &colorName, bool fill) {
  uint16_t color = getColorByName(colorName);
  if (fill) tft.fillRect(x, y, w, h, color);
  else tft.drawRect(x, y, w, h, color);
}

void showMessage(const String &line1, const String &line2 = "");

void loadFolder(const String &path) {
  String normalizedPath = normalizePath(path);
  entryCount = 0;
  currentIndex = 0;
  listOffset = 0;

  File dir = SD.open(normalizedPath);
  if (!dir || !dir.isDirectory()) {
    showMessage("Failed to open folder", normalizedPath);
    return;
  }

  if (normalizedPath != "/") {
    String parentPath = "/";
    int slash = normalizedPath.lastIndexOf('/');
    if (slash > 0) parentPath = normalizedPath.substring(0, slash);
    entries[entryCount++] = {"..", parentPath, true, 0, TYPE_DIR};
  }

  File file = dir.openNextFile();
  while (file && entryCount < MAX_ENTRIES) {
    String rawName = String(file.name());
    String childPath;
    if (rawName.startsWith("/")) {
      childPath = normalizePath(rawName);
    } else {
      childPath = joinPath(normalizedPath, rawName);
    }
    String childName = trimPath(rawName);
    bool isDir = file.isDirectory();
    entries[entryCount].name = childName;
    entries[entryCount].path = childPath;
    entries[entryCount].isDir = isDir;
    entries[entryCount].size = file.size();
    entries[entryCount].type = detectFileType(childName, isDir);
    entryCount++;
    file.close();
    file = dir.openNextFile();
  }
  dir.close();

  qsort(entries, entryCount, sizeof(FileEntry), compareEntries);
  prevIndex = -1;
  drawFileList();
}

void drawItem(int idx, bool selected) {
  int i = idx - listOffset;
  if (i < 0 || i >= 6) return;
  
  int y = WIN_TITLE_H + 8 + i * 40;
  
  if (selected) {
    tft.fillRect(4, y, 232, 36, XP_SELECT_BG);
  } else {
    tft.fillRect(4, y, 232, 36, XP_WINDOW_BG);
  }
  
  tft.setCursor(8, y + 8);
  tft.setTextSize(1);
  tft.setTextColor(selected ? XP_SELECT_FG : XP_BLACK);
  tft.print(getFileTag(entries[idx].type));
  tft.print(' ');
  tft.print(entries[idx].name);
  
  if (!entries[idx].isDir) {
    tft.setCursor(8, y + 22);
    tft.setTextColor(selected ? RGB(180,180,180) : XP_GRAY_TEXT);
    tft.print(entries[idx].size);
    tft.print(" bytes");
  }
}

void updateSelection() {
  int visible = 6;
  if (currentIndex < listOffset) listOffset = currentIndex;
  if (currentIndex >= listOffset + visible) listOffset = currentIndex - visible + 1;
  
  if (prevIndex >= listOffset && prevIndex < listOffset + visible && prevIndex != currentIndex) {
    drawItem(prevIndex, false);
  }
  
  if (currentIndex != prevIndex) {
    drawItem(currentIndex, true);
  }
  
  prevIndex = currentIndex;
}

void drawFileList() {
  drawHeader();
  tft.fillRect(0, WIN_TITLE_H, 240, 304 - WIN_TITLE_H, XP_WINDOW_BG);
  
  tft.fillRect(0, WIN_TITLE_H, 240, 1, XP_BTN_HIGHLIGHT);
  tft.fillRect(0, WIN_TITLE_H, 1, 304 - WIN_TITLE_H, XP_BTN_HIGHLIGHT);
  tft.fillRect(239, WIN_TITLE_H, 1, 304 - WIN_TITLE_H, XP_BTN_SHADOW);
  tft.fillRect(0, 303, 240, 1, XP_BTN_SHADOW);
  
  tft.fillRect(0, WIN_TITLE_H + 1, 240, 1, XP_WHITE);
  tft.fillRect(1, WIN_TITLE_H + 1, 238, 1, XP_BTN_FACE);
  
  int visible = 6;
  if (currentIndex < listOffset) listOffset = currentIndex;
  if (currentIndex >= listOffset + visible) listOffset = currentIndex - visible + 1;

  for (int i = 0; i < visible; i++) {
    int idx = i + listOffset;
    int y = WIN_TITLE_H + 8 + i * 40;
    if (idx >= entryCount) continue;
    
    if (idx == currentIndex) {
      tft.fillRect(4, y, 232, 36, XP_SELECT_BG);
    } else {
      tft.fillRect(4, y, 232, 36, XP_WINDOW_BG);
    }
    
    tft.setCursor(8, y + 8);
    tft.setTextSize(1);
    tft.setTextColor(idx == currentIndex ? XP_SELECT_FG : XP_BLACK);
    tft.print(getFileTag(entries[idx].type));
    tft.print(' ');
    tft.print(entries[idx].name);
    
    if (!entries[idx].isDir) {
      tft.setCursor(8, y + 22);
      tft.setTextColor(idx == currentIndex ? RGB(180,180,180) : XP_GRAY_TEXT);
      tft.print(entries[idx].size);
      tft.print(" bytes");
    }
  }

  String footer = "MENU=Options | SELECT=Open | START=Scan";
  if (copySource.length()) footer = (cutMode ? "Cut ready" : "Copy ready") + String(" | ") + footer;
  drawFooter(footer);
}

void showMessage(const String &line1, const String &line2) {
  tft.fillScreen(XP_DESKTOP);
  
  int winW = 200;
  int winH = line2.length() > 0 ? 100 : 70;
  int winX = (240 - winW) / 2;
  int winY = (320 - winH) / 2 - 20;
  
  tft.fillRect(winX, winY, winW, winH, XP_WINDOW_BG);
  tft.drawRect(winX, winY, winW, winH, XP_BTN_SHADOW);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_WINDOW_BORDER);
  
  drawXPTitleBar("Message", winY + 2);
  
  tft.fillRect(winX + 8, winY + WIN_TITLE_H + 10, 24, 24, XP_BTN_FACE);
  tft.drawRect(winX + 8, winY + WIN_TITLE_H + 10, 24, 24, XP_BTN_SHADOW);
  tft.setTextColor(XP_BLACK);
  tft.setCursor(winX + 15, winY + WIN_TITLE_H + 17);
  tft.print("!");
  
  tft.setCursor(winX + 40, winY + WIN_TITLE_H + 12);
  tft.setTextSize(1);
  tft.setTextColor(XP_BLACK);
  tft.print(line1);
  if (line2.length() > 0) {
    tft.setCursor(winX + 40, winY + WIN_TITLE_H + 26);
    tft.setTextColor(XP_BLUE);
    tft.print(line2);
  }
  
  drawXPButton(winX + winW - 80, winY + winH - 30, 70, 22, "OK", false);
  
  while (!buttonState(KEY_START)) delay(10);
  waitForButtonRelease(KEY_START);
  drawFileList();
}

String askText(const String &label, String value) {
  int cursor = value.length();
  if (cursor == 0) {
    value = "new_name";
    cursor = value.length();
  }
  int charIndex = 0;
  const char choices[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-.";
  int lenChoices = strlen(choices);

  while (true) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(4, 4);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.print(label);
    tft.setCursor(4, 24);
    tft.setTextSize(2);
    tft.print(value);
    tft.drawFastHLine(4, 68, 232, ST77XX_YELLOW);
    tft.setCursor(4, 80);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.print("UP/DOWN = change char");
    tft.setCursor(4, 96);
    tft.print("LEFT/RIGHT = move cursor");
    tft.setCursor(4, 112);
    tft.print("SELECT = Save  MENU = Cancel");

    if (buttonState(KEY_UP)) {
      int valueIndex = cursor == 0 ? 0 : (cursor < (int)value.length() ? cursor : (int)value.length() - 1);
      char c = value[valueIndex];
      int pos = strchr(choices, c) ? strchr(choices, c) - choices : 0;
      pos = (pos + 1) % lenChoices;
      if (value.length()) value[valueIndex] = choices[pos];
      else value += choices[pos];
      waitForButtonRelease(KEY_UP);
    }
    if (buttonState(KEY_DOWN)) {
      int valueIndex = cursor == 0 ? 0 : (cursor < (int)value.length() ? cursor : (int)value.length() - 1);
      char c = value[valueIndex];
      int pos = strchr(choices, c) ? strchr(choices, c) - choices : 0;
      pos = (pos - 1 + lenChoices) % lenChoices;
      if (value.length()) value[valueIndex] = choices[pos];
      else value += choices[pos];
      waitForButtonRelease(KEY_DOWN);
    }
    if (buttonState(KEY_LEFT)) {
      if (cursor > 0) cursor--;
      waitForButtonRelease(KEY_LEFT);
    }
    if (buttonState(KEY_RIGHT)) {
      if (cursor < value.length() - 1) cursor++;
      waitForButtonRelease(KEY_RIGHT);
    }
    if (buttonState(KEY_START)) {
      waitForButtonRelease(KEY_START);
      return value;
    }
    if (buttonState(KEY_A)) {
      waitForButtonRelease(KEY_A);
      return "";
    }
    delay(50);
  }
}

bool copyFile(const String &src, const String &dst) {
  File source = SD.open(src, FILE_READ);
  if (!source) return false;
  File dest = SD.open(dst, FILE_WRITE);
  if (!dest) {
    source.close();
    return false;
  }
  uint8_t buffer[128];
  while (source.available()) {
    int len = source.read(buffer, sizeof(buffer));
    dest.write(buffer, len);
  }
  source.close();
  dest.close();
  return true;
}

void editFile(const String &path) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    showMessage("Cannot open file for editing", path);
    return;
  }
  editBuffer = "";
  while (file.available()) {
    char c = file.read();
    if (c != '\r') editBuffer += c;
  }
  file.close();
  editMode = true;
  showTextEditor();
}

void showTextEditor() {
  int cursorPos = 0;
  int topLine = 0;
  const int maxLines = 15;
  String lines[maxLines];
  int lineCount = 0;
  String currentLine = "";
  for (int i = 0; i < editBuffer.length(); i++) {
    if (editBuffer[i] == '\n') {
      if (lineCount < maxLines) lines[lineCount++] = currentLine;
      currentLine = "";
    } else {
      currentLine += editBuffer[i];
    }
  }
  if (currentLine.length() && lineCount < maxLines) lines[lineCount++] = currentLine;

  int cursorX = 0; // cursor position within the current line
  const char choices[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-.,;:!?()[]{}@#$%^&*+=<>|/\\~`'\"";
  int lenChoices = strlen(choices);

  while (true) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(4, 4);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Editing file");
    for (int i = 0; i < maxLines && topLine + i < lineCount; i++) {
      tft.setCursor(4, 28 + i * 18);
      tft.setTextColor(ST77XX_CYAN);
      String displayLine = lines[topLine + i];
      if (topLine + i == cursorPos) {
        // Insert cursor indicator
        displayLine = displayLine.substring(0, cursorX) + "_" + displayLine.substring(cursorX);
      }
      tft.print(displayLine);
    }
    drawFooter("UP/DN=char L/R=cursor B=del OPT=space START=save A=exit");

    if (buttonState(KEY_UP)) {
      if (cursorPos > 0) {
        cursorPos--;
        if (cursorPos < topLine) topLine = cursorPos;
        cursorX = min(cursorX, (int)lines[cursorPos].length());
      } else {
        // Cycle character up at cursor position
        if (cursorX <= lines[cursorPos].length()) {
          if (cursorX < lines[cursorPos].length()) {
            char c = lines[cursorPos][cursorX];
            int pos = -1;
            for (int j = 0; j < lenChoices; j++) {
              if (choices[j] == c) {
                pos = j;
                break;
              }
            }
            if (pos >= 0) {
              pos = (pos + 1) % lenChoices;
              lines[cursorPos][cursorX] = choices[pos];
            }
          } else {
            // Append new character
            lines[cursorPos] += choices[0];
            cursorX = lines[cursorPos].length() - 1;
          }
        }
      }
      waitForButtonRelease(KEY_UP);
    }
    if (buttonState(KEY_DOWN)) {
      if (cursorPos < lineCount - 1) {
        cursorPos++;
        if (cursorPos >= topLine + maxLines) topLine = cursorPos - maxLines + 1;
        cursorX = min(cursorX, (int)lines[cursorPos].length());
      } else {
        // Cycle character down at cursor position
        if (cursorX <= lines[cursorPos].length()) {
          if (cursorX < lines[cursorPos].length()) {
            char c = lines[cursorPos][cursorX];
            int pos = -1;
            for (int j = 0; j < lenChoices; j++) {
              if (choices[j] == c) {
                pos = j;
                break;
              }
            }
            if (pos >= 0) {
              pos = (pos - 1 + lenChoices) % lenChoices;
              lines[cursorPos][cursorX] = choices[pos];
            }
          } else {
            // Append new character
            lines[cursorPos] += choices[0];
            cursorX = lines[cursorPos].length() - 1;
          }
        }
      }
      waitForButtonRelease(KEY_DOWN);
    }
    if (buttonState(KEY_LEFT)) {
      if (cursorX > 0) cursorX--;
      waitForButtonRelease(KEY_LEFT);
    }
    if (buttonState(KEY_RIGHT)) {
      if (cursorX < lines[cursorPos].length()) cursorX++;
      waitForButtonRelease(KEY_RIGHT);
    }
    if (buttonState(KEY_B)) {
      // Delete character at cursor (Symbian: B=Delete)
      if (cursorX < lines[cursorPos].length()) {
        lines[cursorPos].remove(cursorX, 1);
        if (cursorX > lines[cursorPos].length() - 1) cursorX = max(0, (int)lines[cursorPos].length() - 1);
      }
      waitForButtonRelease(KEY_A);
    }
    if (buttonState(KEY_OPTION)) {
      // Insert space at cursor (Symbian: doi sang OPTION)
      lines[cursorPos] = lines[cursorPos].substring(0, cursorX) + " " + lines[cursorPos].substring(cursorX);
      cursorX++;
      waitForButtonRelease(KEY_B);
    }
    if (buttonState(KEY_START)) {
      waitForButtonRelease(KEY_START);
      // Rebuild editBuffer
      editBuffer = "";
      for (int i = 0; i < lineCount; i++) {
        editBuffer += lines[i] + "\n";
      }
      showMessage("File edited", "Use Save to commit");
      break;
    }
    if (buttonState(KEY_A)) {
      waitForButtonRelease(KEY_A);
      editMode = false;
      drawFileList();
      break;
    }
    delay(50);
  }
}

void saveFile(const String &path) {
  if (!editMode) return;
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    showMessage("Cannot save file", path);
    return;
  }
  file.print(editBuffer);
  file.close();
  editMode = false;
  showMessage("File saved", path);
  loadFolder(currentFolder);
}

void saveFileAs(const String &newPath) {
  if (!editMode) return;
  File file = SD.open(newPath, FILE_WRITE);
  if (!file) {
    showMessage("Cannot save file as", newPath);
    return;
  }
  file.print(editBuffer);
  file.close();
  editMode = false;
  showMessage("File saved as", newPath);
  loadFolder(currentFolder);
}

void showProperties(int index) {
  if (index < 0 || index >= entryCount) return;
  FileEntry &entry = entries[index];
  
  tft.fillScreen(XP_DESKTOP);
  
  int winW = 220;
  int winH = 140;
  int winX = 10;
  int winY = 60;
  
  tft.fillRect(winX, winY, winW, winH, XP_WINDOW_BG);
  tft.drawRect(winX, winY, winW, winH, XP_BTN_SHADOW);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_WINDOW_BORDER);
  drawXPTitleBar("Properties", winY + 2);
  
  tft.setCursor(winX + 12, winY + WIN_TITLE_H + 12);
  tft.setTextSize(1);
  tft.setTextColor(XP_BLACK);
  tft.print("Name: ");
  tft.setTextColor(XP_BLUE);
  tft.print(entry.name);
  
  tft.setCursor(winX + 12, winY + WIN_TITLE_H + 28);
  tft.setTextColor(XP_BLACK);
  tft.print("Type: ");
  tft.setTextColor(XP_BLUE);
  tft.print(entry.isDir ? "Folder" : "File");
  tft.print(" (");
  tft.print(getFileTag(entry.type));
  tft.print(")");
  
  if (!entry.isDir) {
    tft.setCursor(winX + 12, winY + WIN_TITLE_H + 44);
    tft.setTextColor(XP_BLACK);
    tft.print("Size: ");
    tft.setTextColor(XP_BLUE);
    tft.print(entry.size);
    tft.print(" bytes");
  }
  
  tft.setCursor(winX + 12, winY + WIN_TITLE_H + 60);
  tft.setTextColor(XP_BLACK);
  tft.print("Path: ");
  tft.setTextColor(XP_BLUE);
  tft.print(entry.path);
  
  drawXPButton(winX + winW - 80, winY + winH - 32, 70, 22, "OK", false);
  
  while (!buttonState(KEY_START)) delay(10);
  waitForButtonRelease(KEY_START);
  drawFileList();
}

void showTextViewer(const String &path) {
  File file = SD.open(path);
  if (!file) {
    showMessage("Cannot open file");
    return;
  }
  
  tft.fillScreen(XP_DESKTOP);
  
  int winW = 224;
  int winH = 260;
  int winX = 8;
  int winY = 30;
  
  tft.fillRect(winX, winY, winW, winH, XP_WINDOW_BG);
  tft.drawRect(winX, winY, winW, winH, XP_BTN_SHADOW);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_WINDOW_BORDER);
  drawXPTitleBar(path, winY + 2);
  
  int y = winY + WIN_TITLE_H + 8;
  const int lineHeight = 14;
  const int maxLines = 14;
  
  tft.setTextSize(1);
  tft.setTextColor(XP_BLACK);
  
  while (file.available() && y < winY + winH - 40) {
    String line = file.readStringUntil('\n');
    if (line.length() > 28) line = line.substring(0, 28);
    tft.setCursor(winX + 8, y);
    tft.print(line);
    y += lineHeight;
  }
  file.close();
  
  drawXPButton(winX + winW - 80, winY + winH - 30, 70, 22, "Close", false);
  
  while (!buttonState(KEY_START)) delay(10);
  waitForButtonRelease(KEY_START);
  drawFileList();
}

void scanDirectory(const String &path, uint32_t &count) {
  String normalizedPath = normalizePath(path);
  File dir = SD.open(normalizedPath);
  if (!dir || !dir.isDirectory()) return;
  File file = dir.openNextFile();
  while (file) {
    String rawName = String(file.name());
    String childPath;
    if (rawName.startsWith("/")) {
      childPath = normalizePath(rawName);
    } else {
      childPath = joinPath(normalizedPath, rawName);
    }
    if (file.isDirectory()) {
      scanDirectory(childPath, count);
    } else {
      count++;
    }
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
}

void showFolderScan() {
  uint32_t count = 0;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(4, 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Scanning SD card...");
  drawFooter("Please wait...");

  scanDirectory(normalizePath(currentFolder), count);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(4, 40);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.print("Scan complete");
  tft.setCursor(4, 80);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Files found: ");
  tft.print(count);
  drawFooter("START to continue");
  while (!buttonState(KEY_START)) delay(10);
  waitForButtonRelease(KEY_START);
  drawFileList();
}

void luaSetVar(const String &name, const VarValue &value) {
  for (int i = 0; i < luaVarCount; i++) {
    if (luaVars[i].name == name) {
      luaVars[i].value = value;
      return;
    }
  }
  if (luaVarCount < 20) {
    luaVars[luaVarCount].name = name;
    luaVars[luaVarCount].value = value;
    luaVarCount++;
  }
}

bool luaHasVar(const String &name) {
  for (int i = 0; i < luaVarCount; i++) {
    if (luaVars[i].name == name) return true;
  }
  return false;
}

VarValue luaGetVar(const String &name) {
  for (int i = 0; i < luaVarCount; i++) {
    if (luaVars[i].name == name) return luaVars[i].value;
  }
  return VarValue{false, "", 0};
}

VarValue evalExpression(const String &expr);

void displayLuaLine(int &lineY, const String &text) {
  if (lineY > 280) {
    tft.fillScreen(XP_WINDOW_BG);
    lineY = 4;
  }
  tft.setCursor(4, lineY);
  tft.setTextSize(1);
  tft.setTextColor(RGB(0,0,200));
  tft.print(text);
  lineY += 14;
}

VarValue evalExpression(const String &expr) {
  String token = expr;
  token.trim();
  if (token.startsWith("\"") && token.endsWith("\"")) {
    return VarValue{true, token.substring(1, token.length() - 1), 0};
  }
  int concatPos = token.indexOf("..");
  if (concatPos >= 0) {
    VarValue a = evalExpression(token.substring(0, concatPos));
    VarValue b = evalExpression(token.substring(concatPos + 2));
    String aStr = a.isString ? a.str : String(a.num);
    String bStr = b.isString ? b.str : String(b.num);
    return VarValue{true, aStr + bStr, 0};
  }
  int pos = -1;
  char op = 0;
  for (int i = 0; i < token.length(); i++) {
    if (token[i] == '+' || token[i] == '-' || token[i] == '*' || token[i] == '/') {
      op = token[i];
      pos = i;
      break;
    }
  }
  if (pos >= 0) {
    VarValue a = evalExpression(token.substring(0, pos));
    VarValue b = evalExpression(token.substring(pos + 1));
    if (op == '+') return VarValue{false, "", a.num + b.num};
    if (op == '-') return VarValue{false, "", a.num - b.num};
    if (op == '*') return VarValue{false, "", a.num * b.num};
    if (op == '/') return VarValue{false, "", b.num == 0 ? 0 : a.num / b.num};
  }
  token.trim();
  if (luaHasVar(token)) return luaGetVar(token);
  if (token.length() > 1 && token[0] == '"' && token[token.length()-1] == '"') {
    return VarValue{true, token.substring(1, token.length()-1), 0};
  }
  return VarValue{false, "", token.toFloat()};
}

bool runLuaScript(const String &path) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  
  String content = "";
  while (file.available()) content += (char)file.read();
  file.close();
  
  runLuaCode(content);
  return true;
}

void handleRunFile(int index) {
  if (index < 0 || index >= entryCount) return;
  FileEntry &entry = entries[index];
  if (entry.isDir) {
    showMessage("Cannot run folder", entry.name);
    return;
  }
  if (entry.type == TYPE_LUA) {
    if (!runLuaScript(entry.path)) showMessage("Lua run failed", entry.path);
  } else if (entry.type == TYPE_PY) {
    showMessage("Python interpreter not available", entry.name);
  } else if (entry.type == TYPE_HTM || entry.type == TYPE_JSON || entry.type == TYPE_SCR || entry.type == TYPE_TXT) {
    showTextViewer(entry.path);
  } else if (entry.type == TYPE_IMG) {
    showMessage("Image viewer placeholder", entry.name);
  } else if (entry.type == TYPE_ROM) {
    showMessage("Retro game UI placeholder", entry.name);
  } else {
    showMessage("Open unsupported file", entry.name);
  }
}

void performMenuAction(int option) {
  if (option < 0 || option >= 14) return;
  FileEntry &entry = entries[currentIndex];
  switch (option) {
    case 0: // Open
      if (entry.isDir) {
        currentFolder = normalizePath(entry.path);
        loadFolder(currentFolder);
      } else if (entry.type == TYPE_LUA) {
        if (!runLuaScript(entry.path)) showMessage("Lua run failed", entry.path);
      } else {
        if (entry.type == TYPE_TXT || entry.type == TYPE_HTM || entry.type == TYPE_JSON || entry.type == TYPE_SCR) {
          showTextViewer(entry.path);
        } else if (entry.type == TYPE_IMG) {
          showMessage("Image viewer placeholder", entry.name);
        } else {
          showMessage("Open not supported", entry.name);
        }
      }
      break;
    case 1: // Run
      handleRunFile(currentIndex);
      break;
    case 2: // Properties
      showProperties(currentIndex);
      break;
    case 3: // Copy
      if (!entry.isDir) {
        copySource = entry.path;
        cutMode = false;
        showMessage("File copied", entry.name);
      } else {
        showMessage("Copy folder not supported", entry.name);
      }
      break;
    case 4: // Cut
      if (!entry.isDir) {
        copySource = entry.path;
        cutMode = true;
        showMessage("File cut ready", entry.name);
      } else {
        showMessage("Cut folder not supported", entry.name);
      }
      break;
    case 5: // Paste
      if (copySource.length() == 0) {
        showMessage("Nothing to paste");
        break;
      }
      {
        String destPath = currentFolder;
        if (!destPath.endsWith("/")) destPath += "/";
        destPath += trimPath(copySource);
        if (SD.exists(destPath)) {
          showMessage("Target exists", destPath);
          break;
        }
        if (copyFile(copySource, destPath)) {
          if (cutMode) {
            SD.remove(copySource);
            copySource = "";
            cutMode = false;
          }
          showMessage("Paste complete", destPath);
          loadFolder(currentFolder);
        } else {
          showMessage("Paste failed", destPath);
        }
      }
      break;
    case 6: // Rename
      if (entry.name == "..") {
        showMessage("Cannot rename parent link");
        break;
      }
      {
        String newName = askText("Rename file/folder", entry.name);
        if (newName.length() == 0) {
          showMessage("Rename canceled");
          break;
        }
        String newPath = currentFolder;
        if (!newPath.endsWith("/")) newPath += "/";
        newPath += newName;
        if (SD.exists(newPath)) {
          showMessage("Name already exists", newName);
        } else {
          if (SD.rename(entry.path, newPath)) {
            showMessage("Renamed to", newName);
            loadFolder(currentFolder);
          } else {
            showMessage("Rename failed", newName);
          }
        }
      }
      break;
    case 7: // Delete
      if (entry.name == "..") {
        showMessage("Cannot delete parent link");
        break;
      }
      if (SD.remove(entry.path)) {
        showMessage("Deleted", entry.name);
        loadFolder(currentFolder);
      } else {
        showMessage("Delete failed", entry.name);
      }
      break;
    case 8: // New Folder
      {
        String folderName = askText("New folder name", "NewFolder");
        if (folderName.length()) {
          String path = currentFolder;
          if (!path.endsWith("/")) path += "/";
          path += folderName;
          if (SD.mkdir(path)) {
            showMessage("Folder created", folderName);
            loadFolder(currentFolder);
          } else {
            showMessage("Create failed", folderName);
          }
        }
      }
      break;
    case 9: // Scan entire SD
      showFolderScan();
      break;
    case 10: // Edit file
      if (!entry.isDir && (entry.type == TYPE_TXT || entry.type == TYPE_LUA || entry.type == TYPE_PY || entry.type == TYPE_HTM || entry.type == TYPE_JSON || entry.type == TYPE_SCR)) {
        editFile(entry.path);
      } else {
        showMessage("Cannot edit this file type", entry.name);
      }
      break;
    case 11: // Save file
      if (editMode) {
        saveFile(entry.path);
      } else {
        showMessage("No file open for editing");
      }
      break;
    case 12: // Save as
      if (editMode) {
        String newName = askText("Save as", entry.name);
        if (newName.length()) {
          String newPath = currentFolder;
          if (!newPath.endsWith("/")) newPath += "/";
          newPath += newName;
          saveFileAs(newPath);
        }
      } else {
        showMessage("No file open for editing");
      }
      break;
    case 13: // Run directly
      if (!entry.isDir) {
        if (entry.type == TYPE_LUA) {
          if (!runLuaScript(entry.path)) showMessage("Lua run failed", entry.path);
        } else if (entry.type == TYPE_PY) {
          showMessage("Python interpreter not available", entry.name);
        } else if (entry.type == TYPE_ROM) {
          showMessage("Retro game UI placeholder", entry.name);
        } else {
          showMessage("Cannot run this file type", entry.name);
        }
      } else {
        showMessage("Cannot run folder");
      }
      break;
  }
}

int menuWinX, menuWinY, menuWinW, menuWinH;
const char** menuItems;
int menuItemCount = 0;
int menuSelIndex = 0;
int menuPrevIndex = -1;

void drawMenuItem(int idx) {
  int itemY = menuWinY + WIN_TITLE_H + 8 + idx * 24;
  if (idx == menuSelIndex) {
    tft.fillRect(menuWinX + 4, itemY, menuWinW - 8, 22, XP_SELECT_BG);
    tft.setCursor(menuWinX + 12, itemY + 5);
    tft.setTextSize(1);
    tft.setTextColor(XP_SELECT_FG);
    tft.print(menuItems[idx]);
  } else if (idx == menuPrevIndex) {
    tft.fillRect(menuWinX + 4, itemY, menuWinW - 8, 22, XP_WINDOW_BG);
    tft.setCursor(menuWinX + 12, itemY + 5);
    tft.setTextSize(1);
    tft.setTextColor(XP_BLACK);
    tft.print(menuItems[idx]);
  }
}

void drawMenuFull() {
  tft.fillRect(menuWinX, menuWinY, menuWinW, menuWinH, XP_WINDOW_BG);
  tft.drawRect(menuWinX, menuWinY, menuWinW, menuWinH, XP_BTN_SHADOW);
  tft.drawRect(menuWinX + 1, menuWinY + 1, menuWinW - 2, menuWinH - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(menuWinX + 1, menuWinY + 1, menuWinW - 2, menuWinH - 2, XP_WINDOW_BORDER);
  drawXPTitleBar("Options", menuWinY + 2);
  
  for (int i = 0; i < menuItemCount; i++) {
    drawMenuItem(i);
  }
  
  int btnY = menuWinY + menuWinH - 32;
  drawXPButton(menuWinX + menuWinW/2 - 80, btnY, 70, 22, "OK", false);
  drawXPButton(menuWinX + menuWinW/2 + 10, btnY, 70, 22, "Cancel", false);
}

void showLuaEditor(const String &path) {
  File file = SD.open(path, FILE_READ);
  String content = "";
  if (file) {
    while (file.available()) content += (char)file.read();
    file.close();
  }
  
  tft.fillScreen(XP_DESKTOP);
  
  int winW = 224;
  int winH = 270;
  int winX = 8;
  int winY = 25;
  
  tft.fillRect(winX, winY, winW, winH, XP_WINDOW_BG);
  tft.drawRect(winX, winY, winW, winH, XP_BTN_SHADOW);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_WINDOW_BORDER);
  drawXPTitleBar("Lua Editor", winY + 2);
  
  int editY = winY + WIN_TITLE_H + 6;
  int editH = winH - WIN_TITLE_H - 50;
  
  const int maxLines = 14;
  const int lineH = 12;
  String lines[maxLines];
  int lineCount = 0;
  
  int start = 0;
  for (int i = 0; i < content.length() && lineCount < maxLines; i++) {
    if (content[i] == '\n' || i == content.length() - 1) {
      lines[lineCount] = content.substring(start, i + 1);
      lines[lineCount].trim();
      lineCount++;
      start = i + 1;
    }
  }
  
  int viewOffset = 0;
  int cursorLine = 0;
  int selectedAction = 0;
  
  while (true) {
    tft.fillRect(winX + 4, editY, winW - 8, editH, RGB(255,255,255));
    
    for (int i = 0; i < maxLines && i + viewOffset < lineCount; i++) {
      int y = editY + 4 + i * lineH;
      tft.setCursor(winX + 8, y);
      tft.setTextSize(1);
      if (i + viewOffset == cursorLine) {
        tft.setTextColor(XP_WHITE, XP_SELECT_BG);
      } else {
        tft.setTextColor(XP_BLACK, RGB(255,255,255));
      }
      tft.print(lines[i + viewOffset]);
    }
    
    tft.setTextColor(XP_BLACK, RGB(255,255,255));
    
    bool runSel = (selectedAction == 0);
    bool saveSel = (selectedAction == 1);
    bool closeSel = (selectedAction == 2);
    
    drawXPButton(winX + 10, winY + winH - 35, 65, 22, "Run", runSel);
    drawXPButton(winX + 85, winY + winH - 35, 65, 22, "Save", saveSel);
    drawXPButton(winX + 160, winY + winH - 35, 60, 22, "Close", closeSel);
    
    drawFooter("LEFT/RIGHT=Action | UP/DOWN=Scroll | SELECT=OK");
    
    if (buttonState(KEY_UP)) {
      if (cursorLine > 0) cursorLine--;
      else if (viewOffset > 0) viewOffset--;
      waitForButtonRelease(KEY_UP);
    }
    if (buttonState(KEY_DOWN)) {
      if (cursorLine < lineCount - 1) {
        if (cursorLine < maxLines - 1) cursorLine++;
        else if (viewOffset < lineCount - maxLines) viewOffset++;
      }
      waitForButtonRelease(KEY_DOWN);
    }
    if (buttonState(KEY_LEFT)) {
      selectedAction = (selectedAction + 2) % 3;
      waitForButtonRelease(KEY_LEFT);
    }
    if (buttonState(KEY_RIGHT)) {
      selectedAction = (selectedAction + 1) % 3;
      waitForButtonRelease(KEY_RIGHT);
    }
    if (buttonState(KEY_START)) {
      waitForButtonRelease(KEY_START);
      if (selectedAction == 0) {
        String fullCode = "";
        for (int i = 0; i < lineCount; i++) fullCode += lines[i] + "\n";
        runLuaCode(fullCode);
        tft.fillScreen(XP_DESKTOP);
        tft.fillRect(winX, winY, winW, winH, XP_WINDOW_BG);
        tft.drawRect(winX, winY, winW, winH, XP_BTN_SHADOW);
        drawXPTitleBar("Lua Editor", winY + 2);
      } else if (selectedAction == 1) {
        File f = SD.open(path, FILE_WRITE);
        if (f) {
          for (int i = 0; i < lineCount; i++) {
            f.println(lines[i]);
          }
          f.close();
          showMessage("File saved", path);
        }
      } else {
        break;
      }
    }
    if (buttonState(KEY_A)) {
      waitForButtonRelease(KEY_A);
      break;
    }
    delay(50);
  }
  drawFileList();
}

void runLuaCode(const String &code) {
  tft.fillScreen(XP_DESKTOP);
  
  int winW = 224;
  int winH = 260;
  int winX = 8;
  int winY = 30;
  
  tft.fillRect(winX, winY, winW, winH, XP_WINDOW_BG);
  tft.drawRect(winX, winY, winW, winH, XP_BTN_SHADOW);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_BTN_HIGHLIGHT);
  tft.drawRect(winX + 1, winY + 1, winW - 2, winH - 2, XP_WINDOW_BORDER);
  drawXPTitleBar("Lua Output", winY + 2);
  
  int drawX = winX + 4;
  int drawY = winY + WIN_TITLE_H + 4;
  int drawW = winW - 8;
  int drawH = winH - WIN_TITLE_H - 50;
  uint16_t currentColor = RGB(0, 0, 160);
  
  tft.fillRect(drawX, drawY, drawW, drawH, RGB(255,255,255));
  
  luaVM.run(code);
  
  const char* out = luaVM.getOutput();
  int i = 0;
  String textOutput = "";
  
  while (out && out[i]) {
    if (out[i] == '\x02') {
      int cmd = out[i+1];
      
      if (cmd == CMD_CLEAR) {
        tft.fillRect(drawX, drawY, drawW, drawH, RGB(255,255,255));
      }
      else if (cmd == CMD_COLOR) {
        currentColor = RGB((uint8_t)out[i+2], (uint8_t)out[i+3], (uint8_t)out[i+4]);
        i += 4;
      }
      else if (cmd == CMD_PIXEL) {
        int px = drawX + (uint8_t)out[i+2];
        int py = drawY + (uint8_t)out[i+3];
        tft.drawPixel(px, py, currentColor);
        i += 3;
      }
      else if (cmd == CMD_LINE) {
        int x1 = drawX + (uint8_t)out[i+2];
        int y1 = drawY + (uint8_t)out[i+3];
        int x2 = drawX + (uint8_t)out[i+4];
        int y2 = drawY + (uint8_t)out[i+5];
        tft.drawLine(x1, y1, x2, y2, currentColor);
        i += 5;
      }
      else if (cmd == CMD_RECT) {
        int x1 = drawX + (uint8_t)out[i+2];
        int y1 = drawY + (uint8_t)out[i+3];
        int x2 = drawX + (uint8_t)out[i+4];
        int y2 = drawY + (uint8_t)out[i+5];
        int w = x2 - x1;
        int h = y2 - y1;
        if (w < 0) { w = -w; x1 = x2; }
        if (h < 0) { h = -h; y1 = y2; }
        tft.drawRect(x1, y1, w, h, currentColor);
        i += 5;
      }
      else if (cmd == CMD_FILL) {
        int x1 = drawX + (uint8_t)out[i+2];
        int y1 = drawY + (uint8_t)out[i+3];
        int x2 = drawX + (uint8_t)out[i+4];
        int y2 = drawY + (uint8_t)out[i+5];
        int w = x2 - x1;
        int h = y2 - y1;
        if (w < 0) { w = -w; x1 = x2; }
        if (h < 0) { h = -h; y1 = y2; }
        tft.fillRect(x1, y1, w, h, currentColor);
        i += 5;
      }
      else if (cmd == CMD_TEXT) {
        int tx = drawX + (uint8_t)out[i+2];
        int ty = drawY + (uint8_t)out[i+3];
        tft.setCursor(tx, ty);
        tft.setTextColor(currentColor);
        tft.setTextSize(1);
        i += 4;
        while (out[i] && out[i] != '\x03') {
          tft.print(out[i]);
          i++;
        }
        i--;
      }
      else if (cmd == CMD_CIRCLE) {
        int cx = drawX + (uint8_t)out[i+2];
        int cy = drawY + (uint8_t)out[i+3];
        int cr = (uint8_t)out[i+4];
        tft.drawCircle(cx, cy, cr, currentColor);
        i += 5;
      }
      
      while (out[i] && out[i] != '\x03') i++;
    }
    else if (out[i] == '\n') {
      textOutput += "\n";
    }
    else {
      textOutput += out[i];
    }
    i++;
  }
  
  if (textOutput.length() > 0) {
    tft.setCursor(winX + 8, winY + winH - 45);
    tft.setTextColor(XP_BLACK);
    tft.setTextSize(1);
    tft.print(textOutput.substring(0, 40));
  }
  
  if (luaVM.hasError()) {
    tft.setCursor(winX + 8, winY + winH - 45);
    tft.setTextColor(RGB(200,0,0));
    tft.setTextSize(1);
    tft.print("Error!");
  }
  
  drawXPButton(winX + winW - 80, winY + winH - 30, 70, 22, "Close", false);
  
  while (!buttonState(KEY_START) && !buttonState(KEY_A)) delay(10);
  waitForButtonRelease(KEY_START);
  waitForButtonRelease(KEY_A);
}

void showMenu() {
  const char *items[] = {
    "Open",
    "Run Lua",
    "Lua Editor",
    "Properties",
    "Copy",
    "Cut",
    "Paste",
    "Rename",
    "Delete",
    "New Folder",
    "Scan SD",
    "Edit file",
    "Save file",
    "Restart"
  };
  
  menuItems = items;
  menuItemCount = 14;
  menuSelIndex = 0;
  menuPrevIndex = -1;
  
  menuWinW = 180;
  menuWinH = 280;
  menuWinX = 30;
  menuWinY = 20;
  
  tft.fillScreen(XP_DESKTOP);
  drawMenuFull();
  drawFooter("UP/DOWN select | SELECT=OK | B=Cancel");
  
  while (true) {
    if (buttonState(KEY_UP)) {
      if (menuSelIndex > 0) {
        menuPrevIndex = menuSelIndex;
        menuSelIndex--;
        drawMenuItem(menuPrevIndex);
        drawMenuItem(menuSelIndex);
      }
      waitForButtonRelease(KEY_UP);
    }
    if (buttonState(KEY_DOWN)) {
      if (menuSelIndex < menuItemCount - 1) {
        menuPrevIndex = menuSelIndex;
        menuSelIndex++;
        drawMenuItem(menuPrevIndex);
        drawMenuItem(menuSelIndex);
      }
      waitForButtonRelease(KEY_DOWN);
    }
    if (buttonState(KEY_START)) {
      waitForButtonRelease(KEY_START);
      if (menuSelIndex == 13) {
        tft.fillScreen(XP_DESKTOP);
        tft.setCursor(50, 150);
        tft.setTextSize(1);
        tft.setTextColor(XP_BLACK);
        tft.print("Restarting...");
        delay(500);
        ESP.restart();
      } else if (menuSelIndex == 1) {
        if (currentIndex >= 0 && currentIndex < entryCount) {
          FileEntry &entry = entries[currentIndex];
          if (entry.type == TYPE_LUA) {
            runLuaScript(entry.path);
          } else {
            showMessage("Not a Lua file", entry.name);
          }
        }
      } else if (menuSelIndex == 2) {
        if (currentIndex >= 0 && currentIndex < entryCount) {
          FileEntry &entry = entries[currentIndex];
          if (entry.type == TYPE_LUA) {
            showLuaEditor(entry.path);
          } else {
            showMessage("Not a Lua file", entry.name);
          }
        }
      } else {
        performMenuAction(menuSelIndex);
      }
      drawFileList();
      break;
    }
    if (buttonState(KEY_A)) {
      waitForButtonRelease(KEY_A);
      drawFileList();
      break;
    }
    delay(50);
  }
}

void initLua() {
  luaVM.reset();
}

void setup() {
  esp_sleep_enable_ext0_wakeup((gpio_num_t)KEY_BOOT, 0);
  
  bool isWakeFromSleep = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
  
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_LEFT, INPUT_PULLUP);
  pinMode(KEY_RIGHT, INPUT_PULLUP);
  pinMode(KEY_MENU, INPUT_PULLUP);
  pinMode(KEY_OPTION, INPUT_PULLUP);
  pinMode(KEY_START, INPUT_PULLUP);
  pinMode(KEY_START, INPUT_PULLUP);
  pinMode(KEY_A, INPUT_PULLUP);
  pinMode(KEY_B, INPUT_PULLUP);
  pinMode(KEY_BOOT, INPUT_PULLUP);

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  SPI.begin(TFT_SCL, -1, TFT_SDA, -1);
  tft.init(240, 320);
  tft.setRotation(2);

  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, -1);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  if (!showBootSplash()) {
    tft.fillScreen(XP_DESKTOP);
    tft.setCursor(10, 140);
    tft.setTextSize(1);
    tft.setTextColor(RGB(200,0,0));
    tft.print("Boot failure: SD card not ready.");
    while (true) delay(100);
  }

  if (!SD.exists("/notes")) {
    SD.mkdir("/notes");
  }

  initLua();
  
  if (isWakeFromSleep) {
    tft.fillRect(0, 285, 240, 18, RGB(200,255,200));
    tft.setCursor(4, 288);
    tft.setTextSize(1);
    tft.setTextColor(RGB(0,128,0));
    tft.print("Wake from sleep - BOOT to sleep again");
  }
  
  loadFolder("/");
}

void loop() {
  if (buttonState(KEY_UP)) {
    if (currentIndex > 0) {
      currentIndex--;
      updateSelection();
    }
    waitForButtonRelease(KEY_UP);
  }
  if (buttonState(KEY_DOWN)) {
    if (currentIndex < entryCount - 1) {
      currentIndex++;
      updateSelection();
    }
    waitForButtonRelease(KEY_DOWN);
  }
  if (buttonState(KEY_LEFT)) {
    if (currentFolder != "/") {
      int slash = currentFolder.lastIndexOf('/');
      if (slash > 0) currentFolder = currentFolder.substring(0, slash);
      else currentFolder = "/";
      loadFolder(currentFolder);
    }
    waitForButtonRelease(KEY_LEFT);
  }
  if (buttonState(KEY_RIGHT) || buttonState(KEY_START)) {
    if (buttonState(KEY_RIGHT)) waitForButtonRelease(KEY_RIGHT);
    if (buttonState(KEY_START)) waitForButtonRelease(KEY_START);
    if (currentIndex >= 0 && currentIndex < entryCount) {
      FileEntry &entry = entries[currentIndex];
      if (entry.isDir) {
        currentFolder = normalizePath(entry.path);
        loadFolder(currentFolder);
      } else {
        if (entry.type == TYPE_LUA) {
          if (!runLuaScript(entry.path)) showMessage("Lua run failed", entry.path);
        } else if (entry.type == TYPE_TXT || entry.type == TYPE_HTM || entry.type == TYPE_JSON || entry.type == TYPE_SCR) {
          showTextViewer(entry.path);
        } else {
          showMessage("Use MENU -> Run for this file type", entry.name);
        }
      }
    }
  }
  if (buttonState(KEY_MENU)) {
    waitForButtonRelease(KEY_MENU);
    showMenu();
  }
  if (buttonState(KEY_OPTION)) {
    waitForButtonRelease(KEY_OPTION);
    showFolderScan();
  }
  
  // B giu = shutdown (giu nut vat ly, Symbian map moi giu chuc nang nguon).
  if (buttonState(KEY_B)) {
    if (keyBPressTime == 0) {
      keyBPressTime = millis();
      keyBShowingMsg = false;
    } else if (millis() - keyBPressTime > HOLD_RESTART_MS) {
      tft.fillScreen(XP_DESKTOP);
      tft.setCursor(50, 150);
      tft.setTextSize(1);
      tft.setTextColor(XP_BLACK);
      tft.print("Shutting down...");
      delay(500);
      ESP.restart();
    } else if (!keyBShowingMsg && millis() - keyBPressTime > 500) {
      tft.fillRect(0, 285, 240, 18, RGB(255,220,220));
      tft.setCursor(4, 288);
      tft.setTextSize(1);
      tft.setTextColor(RGB(180,0,0));
      tft.print("Hold B (3s) to shutdown...");
      keyBShowingMsg = true;
    }
  } else {
    if (keyBShowingMsg) {
      tft.fillRect(0, 285, 240, 18, RGB(216,231,252));
      keyBShowingMsg = false;
    }
    keyBPressTime = 0;
  }

  // A nhan = Back ve thu muc cha/Goodbye; A giu = sleep.
  if (buttonState(KEY_A)) {
    if (keyAPressTime == 0) {
      keyAPressTime = millis();
      keyAShowingMsg = false;
    } else if (millis() - keyAPressTime > HOLD_SLEEP_MS) {
      tft.fillScreen(XP_DESKTOP);
      tft.setCursor(40, 140);
      tft.setTextSize(1);
      tft.setTextColor(XP_BLACK);
      tft.print("Entering sleep mode...");
      delay(500);
      esp_deep_sleep_start();
    } else if (!keyAShowingMsg && millis() - keyAPressTime > 500) {
      tft.fillRect(120, 285, 120, 18, RGB(220,255,220));
      tft.setCursor(124, 288);
      tft.setTextSize(1);
      tft.setTextColor(RGB(0,128,0));
      tft.print("Hold A (3s) sleep");
      keyAShowingMsg = true;
    }
  } else {
    if (keyAShowingMsg) {
      tft.fillRect(120, 285, 120, 18, RGB(216,231,252));
    }
    if (keyAPressTime > 0 && millis() - keyAPressTime < 500) {
      // Nhan A (Back): ve thu muc cha, o goc thi Goodbye + restart.
      if (currentFolder != "/") {
        int slash = currentFolder.lastIndexOf('/');
        if (slash > 0) currentFolder = currentFolder.substring(0, slash);
        else currentFolder = "/";
        loadFolder(currentFolder);
      } else {
        tft.fillScreen(XP_DESKTOP);
        tft.setCursor(40, 150);
        tft.setTextSize(1);
        tft.setTextColor(XP_BLACK);
        tft.print("Goodbye!");
        delay(1000);
        ESP.restart();
      }
    }
    keyAPressTime = 0;
    keyAShowingMsg = false;
  }
  
  delay(50);
}
