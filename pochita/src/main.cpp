#include <Arduino.h>
#include "component/Display.h"
#include <SPI.h>
#include <math.h>
#include "asset/bitmap_64dp.h"
#include "component/Config.h"
#include "component/Keyboard.h"
#include "component/Themes.h"
#include "component/UIManager.h"
#include "component/ButtonManager.h"
#include "component/FileManager.h"
#include "lua_interpreter.h"
#include "retro_go.h"

/* ===== 1. CẤU HÌNH PHẦN CỨNG ===== */
// Driver ST7789 duoc cau hinh bang LovyanGFX trong component/Display.h.

TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(HSPI);

bool mountSDCard(bool formatIfEmpty, uint32_t frequency) {
  return SD.begin(SD_CS, sdSPI, frequency, "/sd", 10, formatIfEmpty);
}

SystemMode currentMode = MODE_SPLASH;

// Include Apps
#include "notes.h"
#include "app_registry.h"
#include "router.h"
#include "shell.h"
#include "blu.h"
#include "sdcard.h"
#include "settings.h"
#include "wbrowser.h"
#include "media_player.h"
#include "paint.h"
#include "retro_menu.h"
#include "bitchat.h"
#include "vm.h"
#include "system_launch.h"

// ---- system app launcher (shared with VM, terminal `ai`, chat `/run`) ------
void drawUnavailableApp(const char *title, const char *details);

int systemAppCount() {
  int n = 0;
  for (size_t i = 0; i < appList.size(); i++)
    if (appList[i].type == APP_SYSTEM) n++;
  return n;
}

const char *systemAppName(int i) {
  int n = 0;
  for (size_t j = 0; j < appList.size(); j++) {
    if (appList[j].type == APP_SYSTEM) {
      if (n == i) return appList[j].name.c_str();
      n++;
    }
  }
  return "";
}

int findSystemAppByName(const String &name) {
  String lower = name;
  lower.toLowerCase();
  lower.trim();
  for (size_t i = 0; i < appList.size(); i++) {
    if (appList[i].type != APP_SYSTEM) continue;
    String n = appList[i].name;
    n.toLowerCase();
    if (n == lower) return appList[i].systemId;
  }
  int id = name.toInt();
  if (id >= 0 && id < 15) {
    for (size_t i = 0; i < appList.size(); i++)
      if (appList[i].type == APP_SYSTEM && appList[i].systemId == id)
        return id;
  }
  return -1;
}

bool launchSystemApp(int id) {
  switch (id) {
    case 0: currentMode = MODE_APP_ROUTER;
            routerState = ROUTER_MAIN;
            routerSel = 0;
            if (WiFi.status() != WL_CONNECTED) {
                SymbianUI::drawMessageScreen("WiFi", SymbianUI::ICON_WIFI,
                                             "Auto-connecting", "Joining saved network",
                                             "", "Cancel");
                autoConnectSavedWiFi();
            }
            drawRouterApp(); return true;
    case 1: currentMode = MODE_APP_SDCARD; initSD(); drawFileManager(); return true;
    case 2: currentMode = MODE_APP_BLU; initBluApp(); return true;
    case 3: currentMode = MODE_APP_SHELL; drawShellApp(); return true;
    case 4: currentMode = MODE_APP_NOTES; drawNotesApp(); return true;
    case 5: currentMode = MODE_APP_MAPS; drawUnavailableApp("LORA", "Add LoRa pins/module first"); return true;
    case 6: currentMode = MODE_APP_IR; drawUnavailableApp("IR", "Add IR TX/RX pins first"); return true;
    case 7: currentMode = MODE_APP_BROWSER; initWBrowser(); return true;
    case 8: currentMode = MODE_APP_SETTINGS; drawSettingsApp(); return true;
    case 9: currentMode = MODE_APP_RADIO; initRadioApp(); return true;
    case 10: currentMode = MODE_APP_MUSIC; initMusicApp(); return true;
    case 11: currentMode = MODE_APP_PAINT; initPaintApp(); return true;
    case 12: currentMode = MODE_APP_RETRO; drawRetroMenu(); return true;
    case 13: currentMode = MODE_APP_CHAT; initChatApp(); return true;
    case 14: currentMode = MODE_APP_VM; drawVmMenu(); return true;
    default: return false;
  }
}

const int APP_ICON_SIZE = 56;

constexpr int S60_HEADER_HEIGHT = SymbianUI::HEADER_H;
constexpr int S60_FOOTER_Y = UiLayout::FOOTER_Y;
constexpr int S60_GRID_COLUMNS = 3;
constexpr int S60_GRID_ROWS = 3;
constexpr int S60_APPS_PER_PAGE = S60_GRID_COLUMNS * S60_GRID_ROWS;

constexpr uint16_t S60_DARK_GREEN = 0x23C4;
constexpr uint16_t S60_MID_GREEN = 0x55C5;
constexpr uint16_t S60_LIGHT_GREEN = 0xA7EC;
constexpr uint16_t S60_LIME = 0xD7F4;

uint16_t s60WallpaperColorAt(int y) {
  y = constrain(y, S60_HEADER_HEIGHT, S60_FOOTER_Y - 1);
  int amount = ::map(y, S60_HEADER_HEIGHT, S60_FOOTER_Y - 1, 0, 255);
  int r = 142 - (amount * 74 / 255);
  int g = 193 - (amount * 82 / 255);
  int b = 58 - (amount * 34 / 255);
  return tft.color565(r, g, b);
}

void drawS60Wallpaper() {
  tft.fillRect(0, S60_HEADER_HEIGHT, UiLayout::WIDTH,
               S60_FOOTER_Y - S60_HEADER_HEIGHT, SymbianUI::BG);
}

void drawS60Header(const char *title) {
  SymbianUI::drawStatusBar();
  SymbianUI::drawTitle(title);
}

void drawS60Softkeys(const char *left, const char *right) {
  SymbianUI::drawSoftkeys(left, right);
}

const char *powerOptions[] = {"Shut down", "Restart", "Cancel"};
int selectedApp = 0;
int selectedPower = 0;

/* ===== 3. HÀM QUẢN LÝ MÀN HÌNH ===== */

void initHardware() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
}

// ... (drawRGBBitmap is fine) ...
void drawRGBBitmap(int32_t x, int32_t y, const uint32_t *bitmap, int32_t w, int32_t h) {
  // ... (content remains same)
  int32_t j, i;
  uint16_t color;
  const uint32_t *p = bitmap;

  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
        uint32_t pixel = pgm_read_dword(p++);
        byte r = (pixel >> 16) & 0xFF;
        byte g = (pixel >> 8) & 0xFF;
        byte b = pixel & 0xFF;
        color = tft.color565(r, g, b);
        tft.pushColor(color);
    }
  }
  tft.endWrite();
}

void drawScaledRGBBitmap(int32_t x, int32_t y, const uint32_t *bitmap, int32_t w, int32_t h, int32_t targetW, int32_t targetH) {
  if (w <= 0 || h <= 0 || targetW <= 0 || targetH <= 0) return;

  tft.startWrite();
  tft.setAddrWindow(x, y, targetW, targetH);
  for (int32_t row = 0; row < targetH; row++) {
    int32_t srcY = (row * h) / targetH;
    const uint32_t *rowPtr = bitmap + (srcY * w);
    for (int32_t col = 0; col < targetW; col++) {
      int32_t srcX = (col * w) / targetW;
      uint32_t pixel = pgm_read_dword(rowPtr + srcX);
      byte r = (pixel >> 16) & 0xFF;
      byte g = (pixel >> 8) & 0xFF;
      byte b = pixel & 0xFF;
      uint16_t color = tft.color565(r, g, b);
      tft.pushColor(color);
    }
  }
  tft.endWrite();
}

// Scale an 0x00RRGGBB bitmap into a sprite (used for flicker-free launcher
// tiles). Transparent pixels (0x000000) are skipped so the sprite background
// shows through.
void blitScaledRGBToSprite(TFT_eSprite &spr, int dx, int dy,
                           const uint32_t *bitmap, int w, int h,
                           int targetW, int targetH) {
  if (w <= 0 || h <= 0 || targetW <= 0 || targetH <= 0) return;
  for (int row = 0; row < targetH; row++) {
    int srcY = (row * h) / targetH;
    const uint32_t *rowPtr = bitmap + (srcY * w);
    for (int col = 0; col < targetW; col++) {
      int srcX = (col * w) / targetW;
      uint32_t pixel = pgm_read_dword(rowPtr + srcX);
      if ((pixel & 0xFFFFFF) == 0) continue;  // transparent
      byte r = (pixel >> 16) & 0xFF;
      byte g = (pixel >> 8) & 0xFF;
      byte b = pixel & 0xFF;
      spr.drawPixel(dx + col, dy + row, spr.color565(r, g, b));
    }
  }
}

void drawSplashScreen() {
  const uint16_t AMBER = SymbianUI::ACCENT;
  const uint16_t GREEN = SymbianUI::GOOD;
  const uint16_t RED = SymbianUI::BAD;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  // Retro boot-log header.
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("PoChiTa Os", 8, 10, 4);
  tft.setTextColor(AMBER, TFT_BLACK);
  tft.drawString("E524546", 12, 40, 1);
  tft.drawString("ESP32 S3", 8, 56, 2);
  tft.setTextColor(GREEN, TFT_BLACK);
  tft.drawString("error: 'java", 150, 40, 1);

  // Boot log lines revealed one after another, mimicking a kernel trace.
  struct BootLine { const char *t; int x; int y; uint16_t c; uint8_t f; int d; };
  const BootLine lines[] = {
    {"System Initializing...",     8,  96, AMBER, 2, 360},
    {"SD Card Scanning...",        8, 118, AMBER, 2, 360},
    {"WiFiTools",                  8, 140, AMBER, 2, 300},
    {"ATTACKS",                    8, 162, RED,   2, 300},
    {"STATIC IP",                 70, 186, RED,   2, 300},
    {"cpu",                       16, 210, GREEN, 1, 180},
    {"Scan BLE",                 140, 208, GREEN, 2, 220},
    {"else if (selected == 3) {",  8, 228, GREEN, 1, 320},
  };
  for (const auto &ln : lines) {
    tft.setTextColor(ln.c, TFT_BLACK);
    tft.drawString(ln.t, ln.x, ln.y, ln.f);
    delay(ln.d);
  }

  // Kernel loading bar (amber fill, amber border).
  tft.setTextColor(AMBER, TFT_BLACK);
  tft.drawString("Loading Kernel...", 8, 258, 2);
  int bx = 8, by = 284, bw = UiLayout::WIDTH - 16, bh = 20;
  tft.drawRect(bx, by, bw, bh, AMBER);
  for (int i = 0; i <= 100; i += 2) {
    int progress = ::map(i, 0, 100, 0, bw - 4);
    tft.fillRect(bx + 2, by + 2, progress, bh - 4, AMBER);
    delay(30);
  }
}

void drawAppGlyph(int x, int y, int size, const AppItem &app, bool selected) {
  (void)selected;
  // Render the app's RGB bitmap icon, fitted into the square while keeping
  // its aspect ratio. Transparent (black) pixels blend into the tile.
  if (app.icon && app.iconWidth > 0 && app.iconHeight > 0) {
    float scale = min((float)size / app.iconWidth,
                      (float)size / app.iconHeight);
    int tw = (int)(app.iconWidth * scale);
    int th = (int)(app.iconHeight * scale);
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    int ix = x + (size - tw) / 2;
    int iy = y + (size - th) / 2;
    drawScaledRGBBitmap(ix, iy, app.icon, app.iconWidth, app.iconHeight, tw, th);
    return;
  }

  // Fallback: simple filled placeholder when no bitmap is attached.
  tft.fillRect(x + 10, y + 12, size - 20, size - 20, TFT_WHITE);
}

void drawAppIcon(int index, bool selected) {
  if (index >= appList.size()) return;

  int page = selectedApp / S60_APPS_PER_PAGE;
  if (index < page * S60_APPS_PER_PAGE ||
      index >= (page + 1) * S60_APPS_PER_PAGE) return;

  int displayIndex = index % S60_APPS_PER_PAGE;
  int col = displayIndex % S60_GRID_COLUMNS;
  int row = displayIndex / S60_GRID_COLUMNS;

  const int gridRight = UiLayout::WIDTH - 5;
  int cellWidth = gridRight / S60_GRID_COLUMNS;
  int cellHeight = (S60_FOOTER_Y - S60_HEADER_HEIGHT - 3) / S60_GRID_ROWS;
  int x = col * cellWidth;
  int y = S60_HEADER_HEIGHT + 2 + row * cellHeight;

  const AppItem &app = appList[index];

  // Compose the whole tile off-screen and blit it in one pass so navigating
  // between icons never shows an intermediate cleared (black) cell -> no
  // visible flicker when pressing the arrow keys.
  TFT_eSprite spr(&tft);
  spr.setColorDepth(16);
  if (spr.createSprite(cellWidth, cellHeight)) {
    spr.fillSprite(SymbianUI::BG);

    int bx = 3, by = 2, bw = cellWidth - 6, bh = cellHeight - 4;
    if (selected) {
      spr.drawRoundRect(bx, by, bw, bh, 8, SymbianUI::FG);
      const int len = 10;
      const uint16_t c = SymbianUI::ACCENT;
      spr.drawFastHLine(bx, by, len, c);
      spr.drawFastVLine(bx, by, len, c);
      spr.drawFastHLine(bx + bw - len, by, len, c);
      spr.drawFastVLine(bx + bw - 1, by, len, c);
      spr.drawFastHLine(bx, by + bh - 1, len, c);
      spr.drawFastVLine(bx, by + bh - len, len, c);
      spr.drawFastHLine(bx + bw - len, by + bh - 1, len, c);
      spr.drawFastVLine(bx + bw - 1, by + bh - len, len, c);
    }

    int iconSize = 40;
    if (app.icon && app.iconWidth > 0 && app.iconHeight > 0) {
      float scale = min((float)iconSize / app.iconWidth,
                        (float)iconSize / app.iconHeight);
      int tw = (int)(app.iconWidth * scale);
      int th = (int)(app.iconHeight * scale);
      if (tw < 1) tw = 1;
      if (th < 1) th = 1;
      int ix = (cellWidth - tw) / 2;
      int iy = 7 + (iconSize - th) / 2;
      blitScaledRGBToSprite(spr, ix, iy, app.icon, app.iconWidth,
                            app.iconHeight, tw, th);
    }

    String label = app.name;
    label.toUpperCase();
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(UiLayout::ellipsize(label, 11), cellWidth / 2,
                   cellHeight - 12, 1);

    spr.pushSprite(x, y);
    spr.deleteSprite();
    return;
  }

  // Fallback (sprite allocation failed): draw directly to the panel.
  tft.fillRoundRect(x + 3, y + 2, cellWidth - 6, cellHeight - 4, 8,
                    SymbianUI::BG);
  if (selected) {
    tft.drawRoundRect(x + 3, y + 2, cellWidth - 6, cellHeight - 4, 8,
                      SymbianUI::FG);
    SymbianUI::drawCornerBrackets(x + 3, y + 2, cellWidth - 6, cellHeight - 4,
                                  SymbianUI::ACCENT, 10);
  }

  int iconSize = 40;
  int iconX = x + (cellWidth - iconSize) / 2;
  int iconY = y + 7;
  drawAppGlyph(iconX, iconY, iconSize, appList[index], selected);

  String label = appList[index].name;
  label.toUpperCase();
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(UiLayout::ellipsize(label, 11),
                 x + cellWidth / 2, y + cellHeight - 12, 1);
}

void drawLauncherContent() {
  drawS60Wallpaper();
  drawS60Header("MAIN");
  drawS60Softkeys("Options", "Power");

  int page = selectedApp / S60_APPS_PER_PAGE;
  int startIdx = page * S60_APPS_PER_PAGE;
  int limit = startIdx + S60_APPS_PER_PAGE;
  if (limit > appList.size()) limit = appList.size();
  
  for (int i = startIdx; i < limit; i++) {
    drawAppIcon(i, (i == selectedApp));
  }
  
  if (appList.size() > S60_APPS_PER_PAGE) {
    int pages = (appList.size() + S60_APPS_PER_PAGE - 1) / S60_APPS_PER_PAGE;
    int railW = 6;
    int railX = UiLayout::WIDTH - railW - 2;
    int trackY = S60_HEADER_HEIGHT + 6;
    int trackH = S60_FOOTER_Y - trackY - 6;
    int thumbH = max(24, trackH / pages);
    int thumbY = trackY + (page * (trackH - thumbH)) / max(1, pages - 1);
    tft.drawRoundRect(railX, trackY, railW, trackH, railW / 2,
                      SymbianUI::ACCENT_DK);
    tft.fillRoundRect(railX, thumbY, railW, thumbH, railW / 2,
                      SymbianUI::ACCENT);
  }
}

void drawPowerMenu() {
  SymbianUI::drawChrome("Power menu", "Select", "Cancel");

  for (int i = 0; i < 3; i++) {
    SymbianUI::drawListRow(76 + i * 42, 36, powerOptions[i],
                           i == selectedPower);
  }
}

/* ===== 4. SETUP & LOOP ===== */

void setup() {
  Serial.begin(115200);
  initHardware();

  // Bring up the LCD before SD/Lua/apps. This mirrors CyberOS and ensures a
  // failure in an optional subsystem cannot leave the panel uninitialized.
  tft.init();
  tft.setRotation(0); // Portrait (240x320)
  tft.invertDisplay(false);
  tft.setBrightness(255);
  Serial.printf("TFT initialized: %dx%d, MOSI=%d SCLK=%d CS=%d DC=%d RST=%d\n",
                tft.width(), tft.height(), TFT_MOSI_PIN, TFT_SCLK_PIN,
                TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

  // Display diagnostics are complete; keep the powered-off state clean so the
  // first visible startup sequence is the Nokia-inspired POCHITA OS screen.
  tft.fillScreen(TFT_BLACK);

  // The SD-dependent services must only run after the dedicated SD SPI bus
  // has been configured and mounted.
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  bool sdReady = mountSDCard();
  Serial.println(sdReady ? "SD Card initialized successfully"
                         : "SD Card initialization failed; apps remain usable");

  initApps();
  initLua();
  if (sdReady) checkAvailableEmulators();
  loadSettings();
  SymbianUI::osSyncTime();  // NTP: clock syncs automatically once WiFi links

  // Cấu hình Input
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_LEFT, INPUT_PULLUP);
  pinMode(KEY_RIGHT, INPUT_PULLUP);
  pinMode(KEY_SELECT, INPUT_PULLUP);
  pinMode(KEY_OPTION, INPUT_PULLUP);
  pinMode(KEY_START, INPUT_PULLUP);
  pinMode(KEY_B, INPUT_PULLUP);
  pinMode(KEY_OPTION, INPUT_PULLUP);
  pinMode(KEY_A, INPUT_PULLUP);

  // Initialize ButtonManager
  buttonManager.begin();

  // Boot automatically: show POCHITA OS, then enter the S60 launcher.
  currentMode = MODE_SPLASH;
  drawSplashScreen();
  currentMode = MODE_LAUNCHER;
  drawLauncherContent();
  // Reconnect after the launcher is visible, preserving the five-second splash.
  // Credentials live in ESP32 NVS and are mirrored to SD for compatibility.
  if (autoConnectSavedWiFi()) drawLauncherContent();
}

void drawUnavailableApp(const char *title, const char *details) {
  SymbianUI::drawChrome(title, "Back", "Home");
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Hardware not configured", UiLayout::CENTER_X, 130, 2);
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString(UiLayout::ellipsize(details, 30), UiLayout::CENTER_X, 158, 1);
}

// Symbian keypad: START(center)=OK, A(top-right)=Back, MENU(top-left)=Home,
// B(bottom-right)=Delete, OPTION=Context, SELECT=mode toggle.
bool isSelectPressed() { return (digitalRead(KEY_START) == LOW); }  // OK (Center)
bool isOptionPressed() { return (digitalRead(KEY_OPTION) == LOW); } // Options (Context)
bool isBackPressed()   { return (digitalRead(KEY_A) == LOW); }      // Back (Top Right)
bool isStartPressed()  { return (digitalRead(KEY_MENU) == LOW); }   // Home (Top Left)

bool isSystemAppMode() {
  return currentMode == MODE_APP_SDCARD || currentMode == MODE_APP_SETTINGS ||
         currentMode == MODE_APP_ROUTER || currentMode == MODE_APP_BROWSER ||
         currentMode == MODE_APP_NOTES || currentMode == MODE_APP_WEBS ||
         currentMode == MODE_APP_BLU || currentMode == MODE_APP_SHELL ||
         currentMode == MODE_APP_MAPS || currentMode == MODE_APP_IR ||
         currentMode == MODE_APP_RADIO || currentMode == MODE_APP_MUSIC ||
         currentMode == MODE_APP_PAINT || currentMode == MODE_APP_RETRO ||
         currentMode == MODE_APP_CHAT || currentMode == MODE_APP_VM;
}

String getCurrentAppName() {
  switch (currentMode) {
    case MODE_APP_SDCARD: return "SD Card";
    case MODE_APP_SETTINGS: return "Settings";
    case MODE_APP_ROUTER: return "WiFi";
    case MODE_APP_BROWSER: return "Browser";
    case MODE_APP_NOTES: return "Notes";
    case MODE_APP_WEBS: return "Retro";
    case MODE_APP_BLU: return "Bluetooth";
    case MODE_APP_SHELL: return "Shell";
    case MODE_APP_MAPS: return "Maps";
    case MODE_APP_IR: return "IR";
    case MODE_APP_RADIO: return "Radio";
    case MODE_APP_MUSIC: return "Music";
    case MODE_APP_PAINT: return "Paint";
    case MODE_APP_RETRO: return "Retro";
    case MODE_APP_CHAT: return "Chat";
    case MODE_APP_VM: return "VM";
    default: return "App";
  }
}

void exitAppToHome(const char *extMsg) {
  keyboard.active = false;
  chatTeardown();
  senseTeardown();
  if (bluSprite) {
    bluSprite->deleteSprite();
    delete bluSprite;
    bluSprite = nullptr;
  }
  currentMode = MODE_LAUNCHER;
  drawLauncherContent();
  if (extMsg && *extMsg) Serial.println(extMsg);
}

bool confirmExitApp(const String &appName) {
  int selection = 1; // 0 = YES, 1 = NO

  SymbianUI::drawChrome("Application", "Select", "Back");

  while (true) {
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_LEFT) || buttonManager.isJustPressed(KEY_UP)) {
      selection = 0;
    }
    if (buttonManager.isJustPressed(KEY_RIGHT) || buttonManager.isJustPressed(KEY_DOWN)) {
      selection = 1;
    }
    if (buttonManager.isJustPressed(KEY_START)) {
      return (selection == 0);
    }
    if (buttonManager.isJustPressed(KEY_A)) {
      return false;
    }

    SymbianUI::drawDialog("Confirm exit", "Close " + appName + "?",
                          "Yes", "No", selection == 0,
                          SymbianUI::ICON_INFO, SymbianUI::WARN);

    delay(60);
  }
}

void loop() {
  static unsigned long backHoldStart = 0;
  static bool backHoldDialogShown = false;
  static unsigned long homeHoldStart = 0;
  static bool homeLongHandled = false;

  buttonManager.update();

  if (currentMode != MODE_POWER_OFF && isSystemAppMode()) {
    if (buttonManager.isPressed(KEY_A)) {
      if (backHoldStart == 0) backHoldStart = millis();
      if (!backHoldDialogShown && millis() - backHoldStart > 1000) {
        backHoldDialogShown = true;
        exitAppToHome("Application closed by long Back");
        return;
      }
    } else {
      backHoldStart = 0;
      backHoldDialogShown = false;
    }
  }

  // KEY_B: Về Home (Short Press) hoặc Power Menu (Long Press 5s)
  if (currentMode != MODE_POWER_OFF && buttonManager.isPressed(KEY_MENU)) {
    if (homeHoldStart == 0) homeHoldStart = millis();
    if (!homeLongHandled && millis() - homeHoldStart >= 1500) {
      homeLongHandled = true;
      currentMode = MODE_POWER_MENU;
      selectedPower = 2;
      drawPowerMenu();
    }
    return;
  } else if (homeHoldStart != 0) {
    bool wasLong = homeLongHandled;
    homeHoldStart = 0;
    homeLongHandled = false;
    if (!wasLong && currentMode != MODE_LAUNCHER &&
        currentMode != MODE_POWER_OFF) {
      exitAppToHome("Returned Home");
      return;
    }
  }

  // A simulated shutdown stays blank. Press START once to reboot the device;
  // normal device startup no longer waits for a long power-button press.
  if (currentMode == MODE_POWER_OFF) {
    if (buttonManager.isJustPressed(KEY_MENU)) ESP.restart();
    delay(20);
    return;
  } 
  
  // 2. Logic App Launcher
  else if (currentMode == MODE_LAUNCHER) {
    // Keep the clock / battery alive while idle on the home screen.
    static unsigned long lastBar = 0;
    if (millis() - lastBar >= 30000) {
      lastBar = millis();
      SymbianUI::refreshStatusBar();
    }

    int oldApp = selectedApp;
    bool moved = false;
    int totalApps = appList.size();
    ButtonAction action = buttonManager.getAction();

    // S60 menu navigation: three columns and four rows per page.
    if (action == ButtonAction::RIGHT) {
      if (selectedApp < totalApps - 1) { selectedApp++; moved = true; } 
    }
    else if (action == ButtonAction::LEFT) {
      if (selectedApp > 0) { selectedApp--; moved = true; } 
    }
    else if (action == ButtonAction::DOWN) {
      if (selectedApp + S60_GRID_COLUMNS < totalApps) {
        selectedApp += S60_GRID_COLUMNS;
        moved = true;
      }
    }
    else if (action == ButtonAction::UP) {
      if (selectedApp - S60_GRID_COLUMNS >= 0) {
        selectedApp -= S60_GRID_COLUMNS;
        moved = true;
      }
    }

    if (moved) {
      if ((selectedApp / S60_APPS_PER_PAGE) !=
          (oldApp / S60_APPS_PER_PAGE)) {
        drawLauncherContent();
      } else {
        drawAppIcon(oldApp, false); 
        drawAppIcon(selectedApp, true);
      }
    }

    // Mở App
    if (action == ButtonAction::SELECT) {
      AppItem &app = appList[selectedApp];
      if (app.type == APP_SYSTEM) {
            launchSystemApp(app.systemId);
      } else {
           // Installed entries are real shortcuts into the file runtime.
           // Lua/ROM/text/image types are dispatched by FileManager.
           String appPath = app.filePath;
           currentMode = MODE_APP_SDCARD;
           initSD();
           fileManager.launchPath(appPath);
      }
    }

    // Right softkey (B) behaves like the S60 Exit command.
    if (action == ButtonAction::B) {
      currentMode = MODE_POWER_MENU;
      selectedPower = 2;
      drawPowerMenu();
      return;
    }
    
    // Uninstall / Options
    if (action == ButtonAction::OPTION) {
        SymbianUI::drawChrome("Options", "Select", "Cancel");
        SymbianUI::drawListRow(80, 40, "Uninstall app", false, "Up");
        SymbianUI::drawListRow(126, 40, "Clear menu", false, "Down");
        SymbianUI::drawListRow(172, 40, "Cancel", false, "Back");
        
        delay(500);
        while(true) {
          buttonManager.update();
          if (buttonManager.isJustPressed(KEY_A)) { drawLauncherContent(); break; }
            
          if (buttonManager.isJustPressed(KEY_UP)) { // Uninstall
                 if (selectedApp < appList.size() && appList[selectedApp].type == APP_INSTALLED) {
                      uninstallApp(selectedApp);
                      selectedApp = 0; 
                      drawLauncherContent();
                      break;
                 } else {
                      tft.setTextColor(TFT_RED);
                      tft.drawString("System App!", 120, 110, 1);
                      delay(1000);
                      drawLauncherContent();
                      break;
                 }
            }
             if (buttonManager.isJustPressed(KEY_DOWN)) { // Clear
                 clearLauncher();
                 selectedApp = 0;
                 drawLauncherContent();
                 break;
            }
        }
    }
  }

  // 3. Logic Power Menu
  else if (currentMode == MODE_POWER_MENU) {
    if (buttonManager.isJustPressed(KEY_DOWN)) {
      selectedPower = (selectedPower + 1) % 3;
      drawPowerMenu();
    }
    if (buttonManager.isJustPressed(KEY_UP)) {
      selectedPower = (selectedPower + 2) % 3;
      drawPowerMenu();
    }
    
    if (buttonManager.isJustPressed(KEY_START)) {
      if (selectedPower == 0) { // SHUT DOWN
        tft.fillScreen(TFT_BLACK);
        digitalWrite(TFT_BL, LOW);
        currentMode = MODE_POWER_OFF;
        delay(500);
      } else if (selectedPower == 1) { // RESTART
        ESP.restart();
      } else { // CANCEL
        currentMode = MODE_LAUNCHER;
        drawLauncherContent();
      }
      delay(300);
    }
    
    if (buttonManager.isJustPressed(KEY_A)) { // Quay lại
        currentMode = MODE_LAUNCHER;
        drawLauncherContent();
    }
  }
  
  // 4. Logic Apps
  else if (currentMode == MODE_APP_NOTES) {
    loopNotes();
  }
  else if (currentMode == MODE_APP_ROUTER) loopRouter();
  else if (currentMode == MODE_APP_SDCARD) loopFileManager();
  else if (currentMode == MODE_APP_BLU) loopBlu();
  else if (currentMode == MODE_APP_SHELL) loopShell();
  else if (currentMode == MODE_APP_MAPS) { }  // LORA - placeholder
  else if (currentMode == MODE_APP_IR) { }  // IR - placeholder
  else if (currentMode == MODE_APP_BROWSER) loopWBrowser();
  else if (currentMode == MODE_APP_SETTINGS) loopSettings();
  else if (currentMode == MODE_APP_RADIO) loopRadioApp();
  else if (currentMode == MODE_APP_MUSIC) loopMusicApp();
  else if (currentMode == MODE_APP_PAINT) loopPaint();
  else if (currentMode == MODE_APP_RETRO) loopRetroMenu();
  else if (currentMode == MODE_APP_CHAT) loopChat();
  else if (currentMode == MODE_APP_VM) loopVm();

  // The decoder must be serviced continuously, including while another
  // screen is open, so radio/music can continue in the background.
  serviceMediaAudio();
}
