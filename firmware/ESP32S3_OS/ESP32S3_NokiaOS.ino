
/*
  ╔══════════════════════════════════════════════════════════════════════════════╗
  ║                    ESP32-S3 NOKIA CLASSIC OS v1.0                              ║
  ║                         Hệ điều hành Nokia phong cách                          ║
  ║                                                                              ║
  ║  Board: ESP32-S3-WROOM-1 (N16R8) - 16MB Flash, 8MB PSRAM                     ║
  ║  Display: ST7789 2.0" 240x320 TFT LCD                                        ║
  ║                                                                              ║
  ║  Features: WiFi, BLE, Media, File Manager, Web Browser, GPIO, Lua, 3D, Apps  ║
  ╚══════════════════════════════════════════════════════════════════════════════╝
*/

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <FS.h>
using namespace fs; // Pass FS to WebServer
#include <SD_MMC.h>
#include <WebServer.h>

#if defined(CONFIG_IDF_TARGET_ESP32)
#include <BluetoothSerial.h>
#else
// Dummy class for ESP32-S3 since it lacks classic BT SPP
class BluetoothSerial {
public:
  void begin(const char* name) {}
  void begin(String name) {}
  bool hasClient() { return false; }
  void disconnect() {}
  void end() {}
};
#endif
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>

// Include custom modules
#include "LuaModule.h"
#include "Engine3D.h"
#include "WebBrowser.h"
#include "AudioModule.h"
#include "CalcApp.h"

// ═══════════════════════════════════════════════════════════════════════════════
// PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════════════════════════

// TFT Display (ST7789 240x320)
#define TFT_LEDK    39    // Backlight LED cathode
#define TFT_DC      47    // Data/Command
#define TFT_CS      14    // Chip Select
#define TFT_SCL     48    // SPI Clock
#define TFT_SDA     12    // SPI Data (MOSI)
#define TFT_RST     3     // Reset

// Button Matrix (10 buttons)
#define KEY_UP      7
#define KEY_DOWN    46
#define KEY_LEFT    45
#define KEY_RIGHT   6
#define KEY_MENU    18
#define KEY_OPTION  8
#define KEY_SELECT  16
#define KEY_START   17
#define KEY_A       15
#define KEY_B       5

// SD Card (SDIO 4-bit mode)
#define SD_D3       10    // CD/DAT3
#define SD_CMD      11    // Command
#define SD_CLK      13    // Clock
#define SD_D0       9     // Data 0

// ═══════════════════════════════════════════════════════════════════════════════
// SYSTEM CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define STATUS_BAR_H  20
#define MENU_ITEM_H   42
#define SOFTKEY_H     25

// Nokia Classic Color Palette
#define C_BG          0x0000  // Black
#define C_TEXT        0xC618  // Silver
#define C_HEADER      0x3A18  // Dark Blue
#define C_SELECT      0x0410  // Selected Blue
#define C_ACCENT      0x07E0  // Nokia Green
#define C_WHITE       0xFFFF  // White
#define C_DARK        0x2104  // Dark Gray
#define C_RED         0xF800  // Red
#define C_YELLOW      0xFFE0  // Yellow

// ═══════════════════════════════════════════════════════════════════════════════
// ENUMS & STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════════

enum AppState {
  APP_BOOT,
  APP_HOME,
  APP_MENU,
  APP_WIFI,
  APP_BLUETOOTH,
  APP_MEDIA,
  APP_FILES,
  APP_WEB,
  APP_GPIO,
  APP_LUA,
  APP_3D,
  APP_SETTINGS,
  APP_CALC,
  APP_NOTES,
  APP_CLOCK,
  APP_GAMES
};

struct MenuItem {
  const char* icon;
  const char* label;
  AppState    app;
  uint16_t    color;
};

struct SystemInfo {
  char deviceName[32];
  uint8_t brightness;
  uint8_t volume;
  bool soundEnabled;
  bool wifiAutoConnect;
  char wifiSSID[64];
  char wifiPass[64];
};

// ═══════════════════════════════════════════════════════════════════════════════
// GLOBAL OBJECTS
// ═══════════════════════════════════════════════════════════════════════════════

TFT_eSPI tft = TFT_eSPI();
BluetoothSerial SerialBT;
WebBrowser browser;
AudioPlayer audio;
Calculator calc;
LuaState lua;

// 3D Engine
Renderer3D* renderer3D = nullptr;
Mesh* mesh3D = nullptr;
Camera camera3D;

// ═══════════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════════

AppState currentApp = APP_BOOT;
AppState previousApp = APP_HOME;

// Menu state
int menuIndex = 0;
int menuOffset = 0;
const int MENU_ITEMS_VISIBLE = 6;

// Button state
uint8_t buttonPins[10] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_MENU, 
                          KEY_OPTION, KEY_SELECT, KEY_START, KEY_A, KEY_B};
bool buttonPressed[10] = {false};
unsigned long lastButtonTime = 0;
#define BUTTON_DEBOUNCE 150

// File manager state
String currentPath = "/";
String fileList[100];
int fileCount = 0;
int fileIndex = 0;

// Settings
SystemInfo sysInfo = {
  .deviceName = "NokiaOS-S3",
  .brightness = 100,
  .volume = 80,
  .soundEnabled = true,
  .wifiAutoConnect = false,
  .wifiSSID = "",
  .wifiPass = ""
};

// Menu definition
MenuItem mainMenu[] = {
  {"[WiFi]", "WiFi Network", APP_WIFI, C_ACCENT},
  {"[BT]",   "Bluetooth", APP_BLUETOOTH, 0x07FF},
  {"[M]",    "Media Player", APP_MEDIA, C_YELLOW},
  {"[F]",    "File Manager", APP_FILES, C_WHITE},
  {"[W]",    "Web Browser", APP_WEB, 0x001F},
  {"[G]",    "GPIO Control", APP_GPIO, C_RED},
  {"[L]",    "Lua Script", APP_LUA, 0xF81F},
  {"[3D]",   "3D Viewer", APP_3D, 0xF800},
  {"[C]",    "Calculator", APP_CALC, C_WHITE},
  {"[N]",    "Notes", APP_NOTES, C_YELLOW},
  {"[T]",    "Clock", APP_CLOCK, C_ACCENT},
  {"[S]",    "Settings", APP_SETTINGS, C_TEXT}
};
const int MAIN_MENU_COUNT = 12;

// ═══════════════════════════════════════════════════════════════════════════════
// SETUP & INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n╔════════════════════════════════════════╗"));
  Serial.println(F("║     ESP32-S3 NOKIA CLASSIC OS v1.0     ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));

  // Initialize PSRAM (8MB Octal)
  initPSRAM();

  // Initialize hardware
  initButtons();
  initTFT();
  initSD();

  // Show boot screen
  bootSequence();

  // Initialize wireless
  initWiFi();
  initBluetooth();

  // Load settings
  loadSettings();

  // Initialize apps
  audio.init();
  renderer3D = new Renderer3D(SCREEN_WIDTH, SCREEN_HEIGHT - 50);

  // Go to home
  currentApp = APP_HOME;
  drawHomeScreen();

  Serial.println(F("System ready!"));
}

void loop() {
  handleInput();

  // App-specific updates
  switch (currentApp) {
    case APP_MEDIA:
      audio.update();
      break;
    case APP_3D:
      update3DViewer();
      break;
    case APP_CLOCK:
      updateClock();
      break;
    default:
      break;
  }

  delay(5);
}

// ═══════════════════════════════════════════════════════════════════════════════
// HARDWARE INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════════

void initPSRAM() {
  if (psramInit()) {
    size_t psramSize = ESP.getPsramSize();
    Serial.printf("PSRAM: %d MB initialized\n", psramSize / 1024 / 1024);

    // Allocate 3D mesh in PSRAM
    mesh3D = (Mesh*)ps_malloc(sizeof(Mesh));
    if (mesh3D) {
      mesh3D->allocate(1000, 3000);
      Serial.println("3D mesh allocated in PSRAM");
    }
  } else {
    Serial.println("PSRAM initialization failed!");
  }
}

void initButtons() {
  for (int i = 0; i < 10; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    buttonPressed[i] = false;
  }
  Serial.println("Buttons initialized");
}

void initTFT() {
  // Setup backlight
  pinMode(TFT_LEDK, OUTPUT);
  digitalWrite(TFT_LEDK, HIGH);

  // Initialize display
  tft.init();
  tft.setRotation(0);  // Portrait
  tft.fillScreen(C_BG);
  tft.setTextDatum(TL_DATUM);

  // Setup SPI
  SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);

  Serial.println("TFT initialized: 240x320 ST7789");
}

void initSD() {
  // Configure SDIO pins
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, -1, -1, SD_D3);

  if (!SD_MMC.begin()) {
    Serial.println("SD Card init failed!");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card detected");
    return;
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card: %llu MB, Type: ", cardSize);

  switch (cardType) {
    case CARD_MMC:  Serial.println("MMC"); break;
    case CARD_SD:   Serial.println("SDSC"); break;
    case CARD_SDHC: Serial.println("SDHC"); break;
    default:        Serial.println("Unknown");
  }
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(sysInfo.deviceName);
  WiFi.setAutoReconnect(sysInfo.wifiAutoConnect);
  Serial.println("WiFi initialized");
}

void initBluetooth() {
  SerialBT.begin(sysInfo.deviceName);
  Serial.println("Bluetooth initialized");
}

// ═══════════════════════════════════════════════════════════════════════════════
// BOOT SEQUENCE
// ═══════════════════════════════════════════════════════════════════════════════

void bootSequence() {
  tft.fillScreen(C_BG);

  // Nokia logo animation
  tft.setTextSize(3);
  tft.setTextColor(C_ACCENT, C_BG);

  for (int i = 0; i < 5; i++) {
    int x = 40 + random(-2, 3);
    int y = 80 + random(-2, 3);
    tft.setCursor(x, y);
    tft.print("NOKIA");
    delay(100);
    tft.fillRect(40, 80, 160, 30, C_BG);
  }

  tft.setCursor(40, 80);
  tft.print("NOKIA");

  // Version info
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT, C_BG);
  tft.setCursor(70, 130);
  tft.print("ESP32-S3 OS v1.0");

  // Progress bar
  int pbY = 200;
  tft.drawRect(20, pbY, 200, 20, C_TEXT);

  const char* steps[] = {
    "Loading PSRAM...",
    "Mounting SD card...",
    "Init WiFi...",
    "Init Bluetooth...",
    "Loading settings...",
    "Ready!"
  };

  for (int i = 0; i < 6; i++) {
    tft.fillRect(25, pbY + 5, (i * 190) / 5, 10, C_ACCENT);
    tft.setCursor(20, pbY + 30);
    tft.fillRect(20, pbY + 25, 200, 15, C_BG);
    tft.print(steps[i]);
    delay(300);
  }

  delay(500);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INPUT HANDLING
// ═══════════════════════════════════════════════════════════════════════════════

void handleInput() {
  for (int i = 0; i < 10; i++) {
    bool state = digitalRead(buttonPins[i]) == LOW;

    if (state && !buttonPressed[i]) {
      if (millis() - lastButtonTime > BUTTON_DEBOUNCE) {
        buttonPressed[i] = true;
        lastButtonTime = millis();
        onButtonPress(i);
      }
    } else if (!state) {
      buttonPressed[i] = false;
    }
  }
}

void onButtonPress(int btn) {
  switch (btn) {
    case 0: handleKeyUp(); break;
    case 1: handleKeyDown(); break;
    case 2: handleKeyLeft(); break;
    case 3: handleKeyRight(); break;
    case 4: handleKeyMenu(); break;
    case 5: handleKeyOption(); break;
    case 6: handleKeySelect(); break;
    case 7: handleKeyStart(); break;
    case 8: handleKeyA(); break;
    case 9: handleKeyB(); break;
  }
}

void handleKeyUp() {
  switch (currentApp) {
    case APP_HOME:
      currentApp = APP_MENU;
      menuIndex = 0;
      drawMenu();
      break;
    case APP_MENU:
      if (menuIndex > 0) {
        menuIndex--;
        if (menuIndex < menuOffset) menuOffset--;
        drawMenu();
      }
      break;
    case APP_FILES:
      if (fileIndex > 0) {
        fileIndex--;
        if (fileIndex < menuOffset) menuOffset--;
        drawFileManager();
      }
      break;
    case APP_3D:
      camera3D.orbit(0, 0.1);
      draw3DViewer();
      break;
    default:
      break;
  }
}

void handleKeyDown() {
  switch (currentApp) {
    case APP_MENU:
      if (menuIndex < MAIN_MENU_COUNT - 1) {
        menuIndex++;
        if (menuIndex >= menuOffset + MENU_ITEMS_VISIBLE) menuOffset++;
        drawMenu();
      }
      break;
    case APP_FILES:
      if (fileIndex < fileCount - 1) {
        fileIndex++;
        if (fileIndex >= menuOffset + MENU_ITEMS_VISIBLE) menuOffset++;
        drawFileManager();
      }
      break;
    case APP_3D:
      camera3D.orbit(0, -0.1);
      draw3DViewer();
      break;
    default:
      break;
  }
}

void handleKeyLeft() {
  switch (currentApp) {
    case APP_3D:
      camera3D.orbit(-0.1, 0);
      draw3DViewer();
      break;
    case APP_MEDIA:
      // Previous track
      break;
    default:
      break;
  }
}

void handleKeyRight() {
  switch (currentApp) {
    case APP_3D:
      camera3D.orbit(0.1, 0);
      draw3DViewer();
      break;
    case APP_MEDIA:
      // Next track
      break;
    default:
      break;
  }
}

void handleKeyMenu() {
  if (currentApp == APP_HOME) {
    currentApp = APP_MENU;
    menuIndex = 0;
    menuOffset = 0;
    drawMenu();
  } else {
    previousApp = currentApp;
    currentApp = APP_HOME;
    drawHomeScreen();
  }
}

void handleKeyOption() {
  switch (currentApp) {
    case APP_FILES:
      showFileOptions();
      break;
    case APP_WIFI:
      scanWiFi();
      break;
    default:
      showSystemMenu();
      break;
  }
}

void handleKeySelect() {
  // SELECT = chuyen che do Game/T9 (giu >600ms). OS nay chua co mode
  // nen phim duoc giu trong de tranh nham voi OK cu.
}

void handleKeyStart() {
  // OK giua (Symbian map moi): mo/chay + media toggle.
  switch (currentApp) {
    case APP_MENU:
      launchApp(mainMenu[menuIndex].app);
      break;
    case APP_FILES:
      openSelectedFile();
      break;
    case APP_WIFI:
      toggleWiFi();
      break;
    case APP_BLUETOOTH:
      toggleBluetooth();
      break;
    case APP_CALC:
      calc.input('=');
      drawCalculator();
      break;
    case APP_LUA:
      runLuaScript();
      break;
    case APP_MEDIA:
      if (audio.isPlaying()) {
        audio.pause();
      } else {
        // Play current file
      }
      drawMediaPlayer();
      break;
    default:
      break;
  }
}

void handleKeyA() {
  // Back (Symbian map moi): ve HOME; giu phim app rieng cua CALC/3D.
  switch (currentApp) {
    case APP_CALC:
      calc.input('+');
      drawCalculator();
      break;
    case APP_3D:
      camera3D.zoom(-0.5);
      draw3DViewer();
      break;
    default:
      currentApp = APP_HOME;
      drawHomeScreen();
      break;
  }
}

void handleKeyB() {
  // B = Delete (Symbian map moi). OS nay khong co truong text
  // nen phim duoc giu trong; Back/Home do A (Back) va MENU dam nhan.
  switch (currentApp) {
    case APP_HOME:
      // Show running apps or recent
      break;
    default:
      break;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SCREEN DRAWING - UI COMPONENTS
// ═══════════════════════════════════════════════════════════════════════════════

void drawStatusBar() {
  // Background
  tft.fillRect(0, 0, SCREEN_WIDTH, STATUS_BAR_H, C_HEADER);

  // Time
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setTextSize(1);
  tft.setCursor(5, 6);

  // Get time from RTC or system
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    tft.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    tft.print("12:00");
  }

  // Battery icon
  int batX = SCREEN_WIDTH - 30;
  int batLevel = 85;  // TODO: Read actual battery
  tft.drawRect(batX, 4, 20, 12, C_WHITE);
  tft.fillRect(batX + 2, 6, (batLevel / 5), 8, C_ACCENT);
  tft.fillRect(batX + 20, 7, 2, 6, C_WHITE);

  // WiFi indicator
  if (WiFi.status() == WL_CONNECTED) {
    tft.setCursor(SCREEN_WIDTH - 70, 6);
    tft.print("WiFi");
  }

  // Bluetooth indicator
  if (SerialBT.hasClient()) {
    tft.setCursor(SCREEN_WIDTH - 100, 6);
    tft.setTextColor(0x07FF, C_HEADER);
    tft.print("BT");
  }
}

void drawSoftKeys(const char* left, const char* right) {
  int y = SCREEN_HEIGHT - SOFTKEY_H;
  tft.fillRect(0, y, SCREEN_WIDTH, SOFTKEY_H, C_BG);

  tft.setTextColor(C_ACCENT, C_BG);
  tft.setTextSize(1);

  tft.setCursor(5, y + 8);
  tft.print(left);

  int16_t tw = tft.textWidth(right);
  tft.setCursor(SCREEN_WIDTH - tw - 5, y + 8);
  tft.print(right);

  // Divider line
  tft.drawFastHLine(0, y, SCREEN_WIDTH, C_DARK);
}

void drawHomeScreen() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  // Large clock (Nokia style)
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  tft.setTextSize(4);
  tft.setTextColor(C_TEXT, C_BG);
  tft.setCursor(30, 80);
  tft.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  // Date
  tft.setTextSize(2);
  tft.setCursor(60, 140);
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  tft.print(days[timeinfo.tm_wday]);

  tft.setCursor(50, 170);
  tft.printf("%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  // Status info
  tft.setTextSize(1);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.setCursor(20, 220);

  if (WiFi.status() == WL_CONNECTED) {
    tft.print("WiFi: ");
    tft.print(WiFi.SSID());
    tft.setCursor(20, 235);
    tft.print(WiFi.localIP());
  } else {
    tft.print("No connection");
  }

  drawSoftKeys("Menu", "Apps");
}

void drawMenu() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  // Header
  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setTextSize(1);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("Main Menu");

  // Menu items
  int startY = STATUS_BAR_H + 30;

  for (int i = 0; i < MENU_ITEMS_VISIBLE && (menuOffset + i) < MAIN_MENU_COUNT; i++) {
    int idx = menuOffset + i;
    int y = startY + i * MENU_ITEM_H;

    // Selection highlight
    if (idx == menuIndex) {
      tft.fillRect(0, y, SCREEN_WIDTH, MENU_ITEM_H, C_SELECT);
      tft.setTextColor(C_WHITE, C_SELECT);
    } else {
      tft.setTextColor(C_TEXT, C_BG);
    }

    // Icon
    tft.fillRect(10, y + 10, 20, 20, (idx == menuIndex) ? C_ACCENT : C_TEXT);

    // Label
    tft.setCursor(40, y + 15);
    tft.print(mainMenu[idx].label);

    // Arrow for selected
    if (idx == menuIndex) {
      tft.setCursor(SCREEN_WIDTH - 20, y + 15);
      tft.print(">");
    }
  }

  // Scrollbar
  if (MAIN_MENU_COUNT > MENU_ITEMS_VISIBLE) {
    int sbHeight = (MENU_ITEMS_VISIBLE * MENU_ITEM_H * MENU_ITEMS_VISIBLE) / MAIN_MENU_COUNT;
    int sbY = startY + (menuOffset * (MENU_ITEMS_VISIBLE * MENU_ITEM_H - sbHeight)) / (MAIN_MENU_COUNT - MENU_ITEMS_VISIBLE);
    tft.fillRect(SCREEN_WIDTH - 4, sbY, 3, sbHeight, C_ACCENT);
  }

  drawSoftKeys("Select", "Back");
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: WIFI MANAGER
// ═══════════════════════════════════════════════════════════════════════════════

void drawWiFiManager() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("WiFi Manager");

  tft.setTextColor(C_TEXT, C_BG);
  tft.setCursor(10, 60);

  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    tft.print("Status: Connected");
    tft.setCursor(10, 80);
    tft.print("SSID: ");
    tft.print(WiFi.SSID());
    tft.setCursor(10, 100);
    tft.print("IP: ");
    tft.print(WiFi.localIP());
    tft.setCursor(10, 120);
    tft.print("RSSI: ");
    tft.print(WiFi.RSSI());
    tft.print(" dBm");
    tft.setCursor(10, 140);
    tft.print("MAC: ");
    tft.print(WiFi.macAddress());
  } else {
    tft.print("Status: Disconnected");
    tft.setCursor(10, 80);
    tft.print("Press OPT to scan");
  }

  // Network list area
  tft.drawRect(5, 170, 230, 100, C_TEXT);
  tft.setCursor(10, 175);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.print("Networks:");

  drawSoftKeys(status == WL_CONNECTED ? "Disconnect" : "Connect", "Scan");
}

void toggleWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  } else {
    WiFi.begin(sysInfo.wifiSSID, sysInfo.wifiPass);
  }
  delay(500);
  drawWiFiManager();
}

void scanWiFi() {
  tft.fillRect(5, 170, 230, 100, C_BG);
  tft.setCursor(10, 200);
  tft.print("Scanning...");

  int n = WiFi.scanNetworks();

  tft.fillRect(5, 170, 230, 100, C_BG);
  tft.setCursor(10, 175);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.print("Networks found: ");
  tft.print(n);

  tft.setTextColor(C_TEXT, C_BG);
  int y = 190;
  for (int i = 0; i < n && i < 4; i++) {
    tft.setCursor(10, y);
    tft.print(WiFi.SSID(i));
    tft.print(" (");
    tft.print(WiFi.RSSI(i));
    tft.print(")");
    y += 15;
  }

  WiFi.scanDelete();
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: BLUETOOTH
// ═══════════════════════════════════════════════════════════════════════════════

void drawBluetooth() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("Bluetooth");

  tft.setTextColor(C_TEXT, C_BG);
  tft.setCursor(10, 60);
  tft.print("Name: ");
  tft.print(sysInfo.deviceName);

  tft.setCursor(10, 80);
  if (SerialBT.hasClient()) {
    tft.setTextColor(C_ACCENT, C_BG);
    tft.print("Status: Connected");
    tft.setTextColor(C_TEXT, C_BG);
    tft.setCursor(10, 100);
    tft.print("Client active");
  } else {
    tft.print("Status: Advertising");
    tft.setCursor(10, 100);
    tft.print("Waiting for connection...");
  }

  // BT data
  tft.setCursor(10, 140);
  tft.print("Data received:");

  drawSoftKeys(SerialBT.hasClient() ? "Disconnect" : "Reset", "Back");
}

void toggleBluetooth() {
  if (SerialBT.hasClient()) {
    SerialBT.disconnect();
  } else {
    SerialBT.end();
    delay(100);
    SerialBT.begin(sysInfo.deviceName);
  }
  drawBluetooth();
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: FILE MANAGER
// ═══════════════════════════════════════════════════════════════════════════════

void refreshFileList() {
  fileCount = 0;
  menuOffset = 0;

  File root = SD_MMC.open(currentPath);
  if (!root) return;

  File file = root.openNextFile();
  while (file && fileCount < 100) {
    String name = String(file.name());
    // Skip hidden files
    if (!name.startsWith(".")) {
      fileList[fileCount] = name;
      fileCount++;
    }
    file = root.openNextFile();
  }
  root.close();
}

void drawFileManager() {
  if (fileCount == 0) refreshFileList();

  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("File Manager");

  // Path bar
  tft.setTextColor(C_ACCENT, C_BG);
  tft.setCursor(5, 50);
  String path = currentPath;
  if (path.length() > 35) path = "..." + path.substring(path.length() - 32);
  tft.print(path);

  // File list
  int startY = 70;
  for (int i = 0; i < MENU_ITEMS_VISIBLE && (menuOffset + i) < fileCount; i++) {
    int idx = menuOffset + i;
    int y = startY + i * 35;

    if (idx == fileIndex) {
      tft.fillRect(0, y, SCREEN_WIDTH, 35, C_SELECT);
      tft.setTextColor(C_WHITE, C_SELECT);
    } else {
      tft.setTextColor(C_TEXT, C_BG);
    }

    String fname = fileList[idx];
    bool isDir = fname.endsWith("/");

    // Icon
    tft.fillRect(10, y + 8, 16, 16, isDir ? C_YELLOW : C_TEXT);

    // Name
    tft.setCursor(35, y + 12);
    if (fname.length() > 25) {
      tft.print(fname.substring(0, 22));
      tft.print("...");
    } else {
      tft.print(fname);
    }
  }

  // Info
  tft.setTextColor(C_DARK, C_BG);
  tft.setCursor(10, 290);
  tft.printf("%d items", fileCount);

  drawSoftKeys("Open", "Options");
}

void openSelectedFile() {
  if (fileCount == 0) return;

  String fname = fileList[fileIndex];
  bool isDir = fname.endsWith("/");

  if (isDir) {
    currentPath += fname;
    fileIndex = 0;
    refreshFileList();
    drawFileManager();
  } else {
    // Open based on extension
    if (fname.endsWith(".obj")) {
      loadOBJ(currentPath + fname);
      currentApp = APP_3D;
      draw3DViewer();
    } else if (fname.endsWith(".lua")) {
      loadLuaFile(currentPath + fname);
      currentApp = APP_LUA;
      drawLuaEditor();
    } else if (fname.endsWith(".txt") || fname.endsWith(".html")) {
      viewTextFile(currentPath + fname);
    } else if (fname.endsWith(".wav") || fname.endsWith(".mod")) {
      audio.playFile(currentPath + fname);
      currentApp = APP_MEDIA;
      drawMediaPlayer();
    }
  }
}

void showFileOptions() {
  // Draw popup menu
  int mx = 30, my = 100, mw = 180, mh = 120;
  tft.fillRect(mx, my, mw, mh, C_DARK);
  tft.drawRect(mx, my, mw, mh, C_TEXT);

  tft.setTextColor(C_WHITE, C_DARK);
  tft.setCursor(mx + 10, my + 10);
  tft.print("Options");

  tft.setTextColor(C_TEXT, C_DARK);
  const char* opts[] = {"Copy", "Move", "Delete", "Rename", "Info"};
  for (int i = 0; i < 5; i++) {
    tft.setCursor(mx + 10, my + 35 + i * 18);
    tft.print(opts[i]);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: 3D VIEWER (Blender)
// ═══════════════════════════════════════════════════════════════════════════════

void loadOBJ(const String& path) {
  if (!mesh3D) return;

  File f = SD_MMC.open(path);
  if (!f) return;

  mesh3D->vertexCount = 0;
  mesh3D->indexCount = 0;

  char line[256];
  while (f.available() && mesh3D->vertexCount < 1000 && mesh3D->indexCount < 3000) {
    int n = f.readBytesUntil('\n', line, 255);
    line[n] = 0;

    if (line[0] == 'v' && line[1] == ' ') {
      float x, y, z;
      sscanf(line, "v %f %f %f", &x, &y, &z);
      mesh3D->vertices[mesh3D->vertexCount++] = Vec3(x, y, z);
    } else if (line[0] == 'f' && line[1] == ' ') {
      int v1, v2, v3;
      // Simple face parsing (assumes triangles)
      if (sscanf(line, "f %d %d %d", &v1, &v2, &v3) == 3) {
        mesh3D->indices[mesh3D->indexCount++] = v1 - 1;
        mesh3D->indices[mesh3D->indexCount++] = v2 - 1;
        mesh3D->indices[mesh3D->indexCount++] = v3 - 1;
      }
    }
  }
  f.close();

  // Center and scale
  if (mesh3D->vertexCount > 0) {
    Vec3 center(0, 0, 0);
    for (int i = 0; i < mesh3D->vertexCount; i++) {
      center = center + mesh3D->vertices[i];
    }
    center = center * (1.0f / mesh3D->vertexCount);

    float maxDist = 0;
    for (int i = 0; i < mesh3D->vertexCount; i++) {
      mesh3D->vertices[i] = mesh3D->vertices[i] - center;
      float d = mesh3D->vertices[i].length();
      if (d > maxDist) maxDist = d;
    }

    if (maxDist > 0) {
      float scale = 2.0f / maxDist;
      for (int i = 0; i < mesh3D->vertexCount; i++) {
        mesh3D->vertices[i] = mesh3D->vertices[i] * scale;
      }
    }
  }

  camera3D = Camera();
  camera3D.position = Vec3(0, 0, 5);
}

void draw3DViewer() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("3D Viewer");

  if (!mesh3D || mesh3D->vertexCount == 0) {
    tft.setTextColor(C_TEXT, C_BG);
    tft.setCursor(20, 150);
    tft.print("No model loaded");
    tft.setCursor(20, 170);
    tft.print("Select .obj file");
  } else {
    // Calculate MVP matrix
    Mat4 model = Mat4::rotationX(0) * Mat4::rotationY(0) * Mat4::scale(1.0f);
    Mat4 view = camera3D.getViewMatrix();
    Mat4 proj = camera3D.getProjectionMatrix((float)SCREEN_WIDTH / (SCREEN_HEIGHT - 50));
    Mat4 mvp = model * view * proj;

    // Render wireframe
    renderer3D->clearZBuffer();
    renderer3D->renderWireframe(tft, *mesh3D, mvp, C_ACCENT);

    // Info
    tft.setTextColor(C_TEXT, C_BG);
    tft.setCursor(10, 280);
    tft.printf("V:%d F:%d", mesh3D->vertexCount, mesh3D->indexCount / 3);
  }

  drawSoftKeys("Reset", "Back");
}

void update3DViewer() {
  // Auto-rotate if needed
  static unsigned long lastRot = 0;
  if (millis() - lastRot > 50) {
    // Optional: auto-rotation
    lastRot = millis();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: LUA INTERPRETER
// ═══════════════════════════════════════════════════════════════════════════════

String currentLuaScript = "";
String luaOutput = "";

void loadLuaFile(const String& path) {
  File f = SD_MMC.open(path);
  if (f) {
    currentLuaScript = "";
    while (f.available()) {
      currentLuaScript += (char)f.read();
    }
    f.close();
  }
}

void drawLuaEditor() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("Lua Editor");

  // Editor area
  tft.fillRect(5, 50, 230, 120, C_DARK);
  tft.setTextColor(C_TEXT, C_DARK);
  tft.setCursor(10, 60);

  // Show script preview
  int line = 0;
  for (int i = 0; i < currentLuaScript.length() && line < 8; i++) {
    if (currentLuaScript[i] == '\n') {
      line++;
      tft.setCursor(10, 60 + line * 14);
    } else if (isPrintable(currentLuaScript[i]) && tft.getCursorX() < 220) {
      tft.print(currentLuaScript[i]);
    }
  }

  // Output area
  tft.fillRect(5, 180, 230, 100, C_BG);
  tft.drawRect(5, 180, 230, 100, C_TEXT);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.setCursor(10, 190);
  tft.print("Output:");
  tft.setTextColor(C_TEXT, C_BG);
  tft.setCursor(10, 210);

  // Show output
  line = 0;
  for (int i = 0; i < luaOutput.length() && line < 4; i++) {
    if (luaOutput[i] == '\n') {
      line++;
      tft.setCursor(10, 210 + line * 14);
    } else if (isPrintable(luaOutput[i]) && tft.getCursorX() < 220) {
      tft.print(luaOutput[i]);
    }
  }

  drawSoftKeys("Run", "Back");
}

void runLuaScript() {
  luaOutput = "";

  // Register ESP32-specific functions
  lua.setGlobal("millis", LuaValue());
  LuaValue millisVal = lua.getGlobal("millis");
  millisVal.type = LUA_TFUNCTION;
  lua.setGlobal("millis", millisVal);

  if (lua.doString(currentLuaScript)) {
    luaOutput = lua.getOutput();
    if (luaOutput.length() == 0) luaOutput = "OK\n";
  } else {
    luaOutput = "Error executing script";
  }

  drawLuaEditor();
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: MEDIA PLAYER
// ═══════════════════════════════════════════════════════════════════════════════

void drawMediaPlayer() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("Media Player");

  // Visualizer
  tft.fillRect(20, 60, 200, 100, C_DARK);
  if (audio.isPlaying()) {
    for (int i = 0; i < 20; i++) {
      int h = random(10, 90);
      tft.fillRect(25 + i * 10, 160 - h, 8, h, C_ACCENT);
    }
  }

  // Progress bar
  tft.drawRect(20, 180, 200, 10, C_TEXT);
  if (audio.isPlaying()) {
    uint32_t pos = audio.getPosition();
    uint32_t dur = audio.getDuration();
    if (dur > 0) {
      int pw = (pos * 196) / (dur * 1000);  // Approximate
      tft.fillRect(22, 182, pw, 6, C_ACCENT);
    }
  }

  // Status
  tft.setTextColor(C_TEXT, C_BG);
  tft.setCursor(20, 210);
  tft.print(audio.isPlaying() ? "Playing" : (audio.isPaused() ? "Paused" : "Stopped"));

  drawSoftKeys(audio.isPlaying() ? "Pause" : "Play", "Stop");
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: CALCULATOR
// ═══════════════════════════════════════════════════════════════════════════════

void drawCalculator() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  calc.draw(tft, 10, 30, 220, 260);

  drawSoftKeys("Clear", "Back");
}

// ═══════════════════════════════════════════════════════════════════════════════
// APP: SETTINGS
// ═══════════════════════════════════════════════════════════════════════════════

void drawSettings() {
  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);
  tft.print("Settings");

  const char* items[] = {
    "Brightness", "Volume", "Sound", "WiFi", 
    "Bluetooth", "Device Name", "Format SD", "About"
  };

  int y = 60;
  for (int i = 0; i < 8; i++) {
    tft.setTextColor(C_TEXT, C_BG);
    tft.setCursor(10, y);
    tft.print(items[i]);

    tft.setCursor(140, y);
    switch (i) {
      case 0: tft.printf("%d%%", sysInfo.brightness); break;
      case 1: tft.printf("%d%%", sysInfo.volume); break;
      case 2: tft.print(sysInfo.soundEnabled ? "On" : "Off"); break;
      case 3: tft.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Off"); break;
      case 4: tft.print(SerialBT.hasClient() ? "On" : "Off"); break;
      case 5: tft.print(sysInfo.deviceName); break;
      case 7: tft.print("v1.0"); break;
    }
    y += 28;
  }

  drawSoftKeys("Change", "Back");
}

// ═══════════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

void launchApp(AppState app) {
  currentApp = app;

  switch (app) {
    case APP_WIFI:
      drawWiFiManager();
      break;
    case APP_BLUETOOTH:
      drawBluetooth();
      break;
    case APP_FILES:
      fileIndex = 0;
      refreshFileList();
      drawFileManager();
      break;
    case APP_MEDIA:
      drawMediaPlayer();
      break;
    case APP_3D:
      draw3DViewer();
      break;
    case APP_LUA:
      drawLuaEditor();
      break;
    case APP_CALC:
      drawCalculator();
      break;
    case APP_SETTINGS:
      drawSettings();
      break;
    default:
      drawHomeScreen();
      break;
  }
}

void viewTextFile(const String& path) {
  tft.fillScreen(C_BG);
  drawStatusBar();

  tft.fillRect(0, STATUS_BAR_H, SCREEN_WIDTH, 25, C_HEADER);
  tft.setTextColor(C_WHITE, C_HEADER);
  tft.setCursor(10, STATUS_BAR_H + 8);

  String fname = path;
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash >= 0) fname = path.substring(lastSlash + 1);
  tft.print(fname);

  File f = SD_MMC.open(path);
  if (f) {
    tft.setTextColor(C_TEXT, C_BG);
    int y = 60;
    int lines = 0;
    while (f.available() && lines < 14 && y < 280) {
      String line = f.readStringUntil('\n');
      tft.setCursor(10, y);
      if (line.length() > 28) line = line.substring(0, 25) + "...";
      tft.print(line);
      y += 16;
      lines++;
    }
    f.close();
  }

  drawSoftKeys("", "Back");
}

void showSystemMenu() {
  // Quick settings popup
}

void updateClock() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000 && currentApp == APP_HOME) {
    drawHomeScreen();
    lastUpdate = millis();
  }
}

void loadSettings() {
  if (SD_MMC.exists("/system/settings.json")) {
    File f = SD_MMC.open("/system/settings.json");
    if (f) {
      StaticJsonDocument<512> doc;
      deserializeJson(doc, f);

      sysInfo.brightness = doc["brightness"] | 100;
      sysInfo.volume = doc["volume"] | 80;
      sysInfo.soundEnabled = doc["sound"] | true;
      strlcpy(sysInfo.wifiSSID, doc["wifi_ssid"] | "", sizeof(sysInfo.wifiSSID));
      strlcpy(sysInfo.wifiPass, doc["wifi_pass"] | "", sizeof(sysInfo.wifiPass));
      strlcpy(sysInfo.deviceName, doc["device_name"] | "NokiaOS-S3", sizeof(sysInfo.deviceName));

      f.close();
    }
  }

  // Apply settings
  analogWrite(TFT_LEDK, (sysInfo.brightness * 255) / 100);
}

void saveSettings() {
  File f = SD_MMC.open("/system/settings.json", FILE_WRITE);
  if (f) {
    StaticJsonDocument<512> doc;
    doc["brightness"] = sysInfo.brightness;
    doc["volume"] = sysInfo.volume;
    doc["sound"] = sysInfo.soundEnabled;
    doc["wifi_ssid"] = sysInfo.wifiSSID;
    doc["wifi_pass"] = sysInfo.wifiPass;
    doc["device_name"] = sysInfo.deviceName;

    serializeJson(doc, f);
    f.close();
  }
}
