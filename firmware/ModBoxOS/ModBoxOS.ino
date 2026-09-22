/*
 * ModBoxOS - Hệ điều hành phong cách Nokia cho ESP32-S3
 * Sửa lỗi biên dịch: TFT_GREY, forward declaration, LEDC, Lua_dofile...
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <LuaWrapper.h>
#include "esp32-hal-ledc.h"   // cho ledcSetup, ledcAttachPin

// ======================== Định nghĩa chân ========================
#define TFT_BL   39
#define TFT_DC   47
#define TFT_CS   14
#define TFT_SCLK 48
#define TFT_MOSI 12
#define TFT_RST  3

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

#define SD_CS       10
#define SD_MOSI     11
#define SD_SCLK     13
#define SD_MISO     9

TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(FSPI);
LuaWrapper lua;

// ======================== Lớp Screen cơ sở ========================
class Screen {
public:
  virtual void init() = 0;
  virtual void draw() = 0;
  virtual void handleKey(uint8_t btn, bool pressed) = 0;
  virtual void update() = 0;
  virtual ~Screen() {}
};

Screen* currentScreen = nullptr;
void setScreen(Screen* newScreen);
Screen* createMainMenu();
Screen* createWiFiScreen();
Screen* createBLEScreen();
Screen* createMediaScreen();
Screen* createFileManagerScreen();
Screen* createWebBrowserScreen();
Screen* createGPIOScreen();
Screen* createLuaScreen();
Screen* createAppsScreen();
Screen* createBlenderScreen();
Screen* createSettingsScreen();

// Button handling
bool lastBtnState[10] = {HIGH};
unsigned long lastDebounceTime[10] = {0};
const int debounceDelay = 50;
enum ButtonIndex {
  BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_MENU,
  BTN_OPTION, BTN_SELECT, BTN_START, BTN_A, BTN_B
};
const uint8_t btnPins[10] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_MENU,
                             KEY_OPTION, KEY_SELECT, KEY_START, KEY_A, KEY_B};

void drawStatusBar() {
  tft.fillRect(0, 0, 240, 20, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setCursor(2, 4);
  tft.print("ModBoxOS");
  if (WiFi.status() == WL_CONNECTED) tft.print(" W");
  tft.setCursor(200, 4);
  tft.print("Bat:--");
}

void setScreen(Screen* newScreen) {
  if (currentScreen) delete currentScreen;
  currentScreen = newScreen;
  if (currentScreen) currentScreen->init();
}

void readButtons(bool* states) {
  for (int i = 0; i < 10; i++) {
    bool reading = digitalRead(btnPins[i]) == LOW;
    if (reading != lastBtnState[i]) lastDebounceTime[i] = millis();
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != states[i]) {
        states[i] = reading;
        if (currentScreen && reading) currentScreen->handleKey(i, true);
        else if (currentScreen && !reading) currentScreen->handleKey(i, false);
      }
    }
    lastBtnState[i] = reading;
  }
}

// ======================== Lớp Screen cơ sở ========================
class Screen {
public:
  virtual void init() = 0;
  virtual void draw() = 0;
  virtual void handleKey(uint8_t btn, bool pressed) = 0;
  virtual void update() = 0;
  virtual ~Screen() {}
};

// ======================== MenuScreen ========================
class MenuScreen : public Screen {
public:
  struct MenuItem {
    const char* name;
    Screen* (*create)();
  };
private:
  MenuItem* items;
  int count;
  int selected;
  const char* title;
public:
  MenuScreen(const char* title, MenuItem* items, int count)
    : items(items), count(count), selected(0), title(title) {}
  void init() override { draw(); }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(2, 24);
    tft.println(title);
    tft.drawLine(0, 38, 240, 38, TFT_DARKGREY);  // sửa TFT_GREY -> TFT_DARKGREY
    for (int i = 0; i < count; i++) {
      uint16_t color = (i == selected) ? TFT_CYAN : TFT_WHITE;
      tft.setTextColor(color, TFT_BLACK);
      tft.setCursor(10, 48 + i * 24);
      tft.print(items[i].name);
    }
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    switch (btn) {
      case BTN_UP: selected = (selected - 1 + count) % count; draw(); break;
      case BTN_DOWN: selected = (selected + 1) % count; draw(); break;
      case BTN_START:  // OK giua (Symbian map moi; SELECT cu la mode)
        if (items[selected].create) setScreen(items[selected].create());
        break;
      case BTN_MENU: setScreen(createMainMenu()); break;
      default: break;
    }
  }
  void update() override {}
};

// ======================== Màn hình WiFi ========================
class WiFiScreen : public Screen {
  enum State { SCAN, CONNECT, INFO };
  State state;
  int networkCount;
  String networks[20];
  int selectedNet;
  String password;
public:
  void init() override { state = SCAN; scanNetworks(); draw(); }
  void scanNetworks() {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 30);
    tft.println("Scanning...");
    networkCount = WiFi.scanNetworks();
    if (networkCount > 20) networkCount = 20;
    for (int i = 0; i < networkCount; i++) networks[i] = WiFi.SSID(i);
    draw();
  }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    if (state == SCAN) {
      tft.setCursor(2, 30);
      tft.println("WiFi Networks:");
      for (int i = 0; i < networkCount; i++) {
        tft.setTextColor((i == selectedNet) ? TFT_CYAN : TFT_WHITE);
        tft.printf("%s\n", networks[i].c_str());
      }
      if (networkCount == 0) tft.println("No networks found.");
    } else if (state == CONNECT) {
      tft.setCursor(2, 30);
      tft.println("Enter password:");
      tft.println(password);
      tft.println("\nPress START to connect");
    } else if (state == INFO) {
      tft.setCursor(2, 30);
      tft.printf("Connected to: %s\nIP: %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    }
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (state == SCAN) {
      if (btn == BTN_UP) { selectedNet = (selectedNet - 1 + networkCount) % networkCount; draw(); }
      else if (btn == BTN_DOWN) { selectedNet = (selectedNet + 1) % networkCount; draw(); }
      else if (btn == BTN_START) { state = CONNECT; password = ""; draw(); }
      else if (btn == BTN_MENU) setScreen(createMainMenu());
    } else if (state == CONNECT) {
      if (btn == BTN_A) { password += 'a'; draw(); }
      else if (btn == BTN_B) { password += 'b'; draw(); }
      else if (btn == BTN_UP && password.length() > 0) { password.remove(password.length()-1); draw(); }
      else if (btn == BTN_START) {
        tft.println("Connecting...");
        WiFi.begin(networks[selectedNet].c_str(), password.c_str());
        int tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries++ < 30) delay(500);
        state = INFO;
        draw();
      }
      else if (btn == BTN_MENU) setScreen(createMainMenu());
    } else if (state == INFO && btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createWiFiScreen() { return new WiFiScreen(); }

// ======================== BLE Screen ========================
class BLEScreen : public Screen {
  bool advertising;
  BLEServer* pServer;
public:
  void init() override {
    BLEDevice::init("ModBoxOS_BLE");
    pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    BLECharacteristic* pCharacteristic = pService->createCharacteristic(
                      "beb5483e-36e1-4688-b7f5-ea07361b26a8",
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic->setValue("Hello from ModBoxOS");
    pService->start();
    advertising = true;
    pServer->getAdvertising()->start();
    draw();
  }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setCursor(2,30);
    tft.println("BLE Advertising: ");
    tft.println(advertising ? "ON" : "OFF");
    tft.println("\nPress OPTION to toggle");
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_OPTION) {
      advertising = !advertising;
      if (advertising) pServer->getAdvertising()->start();
      else pServer->getAdvertising()->stop();
      draw();
    } else if (btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createBLEScreen() { return new BLEScreen(); }

// ======================== Media Screen ========================
class MediaScreen : public Screen {
  String path = "/";
  int fileCount;
  String files[50];
  int selected;
  void listFiles() {
    fileCount = 0;
    File dir = SD.open(path);
    while (File f = dir.openNextFile()) {
      String name = f.name();
      if (!f.isDirectory() && (name.endsWith(".bmp") || name.endsWith(".txt"))) {
        files[fileCount++] = name;
      }
      f.close();
    }
    dir.close();
  }
  void showBMP(String filename) {
    File bmp = SD.open((path + filename).c_str());
    if (!bmp) return;
    bmp.seek(18);
    int width = bmp.read() | (bmp.read() << 8);
    int height = bmp.read() | (bmp.read() << 8);
    bmp.seek(28);
    int bpp = bmp.read() | (bmp.read() << 8);
    if (bpp != 24) { tft.println("Only 24-bit BMP"); bmp.close(); delay(1000); return; }
    bmp.seek(54);
    for (int y = height-1; y >= 0; y--) {
      for (int x = 0; x < width; x++) {
        uint8_t b = bmp.read(); uint8_t g = bmp.read(); uint8_t r = bmp.read();
        if (x < 240 && y < 320) tft.drawPixel(x, y+20, tft.color565(r,g,b));
      }
      int pad = (4 - (width*3)%4) % 4;
      bmp.seek(bmp.position() + pad);
    }
    bmp.close();
  }
  void showText(String filename) {
    File txt = SD.open((path + filename).c_str());
    if (!txt) return;
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 20);
    while (txt.available()) {
      char c = txt.read();
      tft.print(c);
      if (tft.getCursorY() > 300) break;
    }
    txt.close();
    delay(3000);
  }
public:
  void init() override { listFiles(); draw(); }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setCursor(2,30);
    for (int i=0; i<fileCount && i<10; i++) {
      tft.setTextColor((i==selected)?TFT_CYAN:TFT_WHITE);
      tft.println(files[i]);
    }
    if (fileCount == 0) tft.println("No BMP/TXT files");
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_UP) { selected = (selected-1+fileCount)%fileCount; draw(); }
    else if (btn == BTN_DOWN) { selected = (selected+1)%fileCount; draw(); }
    else if (btn == BTN_START) {
      if (files[selected].endsWith(".bmp")) showBMP(files[selected]);
      else if (files[selected].endsWith(".txt")) showText(files[selected]);
      draw();
    } else if (btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createMediaScreen() { return new MediaScreen(); }

// ======================== File Manager ========================
class FileManagerScreen : public Screen {
  String currentPath;
  int selected;
  int itemCount;
  String items[50];
  void refresh() {
    itemCount = 0;
    File dir = SD.open(currentPath);
    items[itemCount++] = "..";
    while (File f = dir.openNextFile()) {
      String prefix = f.isDirectory() ? "[D] " : "[F] ";
      items[itemCount++] = prefix + String(f.name());
      f.close();
    }
    dir.close();
  }
public:
  void init() override { currentPath = "/"; refresh(); draw(); }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setCursor(2, 30);
    tft.println("Path: " + currentPath);
    for (int i=0; i<itemCount && i<10; i++) {
      tft.setTextColor((i==selected)?TFT_CYAN:TFT_WHITE);
      tft.println(items[i]);
    }
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_UP) { selected = (selected-1+itemCount)%itemCount; draw(); }
    else if (btn == BTN_DOWN) { selected = (selected+1)%itemCount; draw(); }
    else if (btn == BTN_START) {
      String name = items[selected].substring(4);
      if (name == "..") {
        int lastSlash = currentPath.lastIndexOf('/');
        if (lastSlash > 0) currentPath = currentPath.substring(0, lastSlash);
        else currentPath = "/";
        refresh(); draw();
      } else if (items[selected].startsWith("[D]")) {
        if (currentPath == "/") currentPath = "/" + name;
        else currentPath = currentPath + "/" + name;
        refresh(); draw();
      } else {
        String fullPath = (currentPath == "/") ? "/" + name : currentPath + "/" + name;
        File f = SD.open(fullPath);
        if (f) {
          tft.fillScreen(TFT_BLACK);
          tft.setCursor(0,20);
          while (f.available()) tft.print((char)f.read());
          f.close();
          delay(3000);
          draw();
        }
      }
    } else if (btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createFileManagerScreen() { return new FileManagerScreen(); }

// ======================== Web Browser ========================
class WebBrowserScreen : public Screen {
  String url = "http://example.com";
  String content;
  void fetch() {
    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code > 0) content = http.getString();
    else content = "Error: " + String(code);
    http.end();
  }
public:
  void init() override { fetch(); draw(); }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setCursor(2,30);
    tft.println("URL: " + url);
    tft.println("Content (first 200 chars):");
    tft.println(content.substring(0, 200));
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_A) { url = "http://google.com"; fetch(); draw(); }
    else if (btn == BTN_B) { url = "http://example.com"; fetch(); draw(); }
    else if (btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createWebBrowserScreen() { return new WebBrowserScreen(); }

// ======================== GPIO Screen ========================
class GPIOScreen : public Screen {
  int selectedPin = 0;
  const int pins[21] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21};
  int pinCount = 21;
  bool outputState[21] = {false};
public:
  void init() override { draw(); }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setCursor(2,30);
    tft.printf("Pin: %d\n", pins[selectedPin]);
    tft.printf("Mode: %s\n", outputState[selectedPin] ? "OUTPUT" : "INPUT");
    tft.printf("State: %d\n", digitalRead(pins[selectedPin]));
    tft.println("\nA: Toggle Out  B: Read");
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_UP) { selectedPin = (selectedPin-1+pinCount)%pinCount; draw(); }
    else if (btn == BTN_DOWN) { selectedPin = (selectedPin+1)%pinCount; draw(); }
    else if (btn == BTN_A) {
      pinMode(pins[selectedPin], OUTPUT);
      outputState[selectedPin] = !outputState[selectedPin];
      digitalWrite(pins[selectedPin], outputState[selectedPin] ? HIGH : LOW);
      draw();
    } else if (btn == BTN_B) {
      int val = digitalRead(pins[selectedPin]);
      tft.fillRect(0, 150, 240, 40, TFT_BLACK);
      tft.setCursor(2, 150);
      tft.printf("Read value: %d", val);
      delay(800);
      draw();
    } else if (btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createGPIOScreen() { return new GPIOScreen(); }

// ======================== Lua Screen (sửa lỗi Lua_dofile) ========================
class LuaScreen : public Screen {
  void runLuaFile(const char* path) {
    if (!SD.exists(path)) {
      tft.println("File not found");
      return;
    }
    File f = SD.open(path);
    String script = "";
    while (f.available()) script += (char)f.read();
    f.close();
    lua.Lua_dostring(&script);
  }
public:
  void init() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setCursor(2,30);
    tft.println("Lua ready.");
    tft.println("A: Run demo print\nB: Run /script.lua");
  }
  void draw() override {}
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_A) {
      String demo = "print('Hello from Lua on ESP32-S3!')";
      lua.Lua_dostring(&demo);
      tft.setCursor(2, 100);
      tft.println("Executed demo");
    } else if (btn == BTN_B) {
      runLuaFile("/script.lua");
      tft.println("Executed /script.lua");
    } else if (btn == BTN_MENU) {
      setScreen(createMainMenu());
    }
    delay(500);
    init();
  }
  void update() override {}
};
Screen* createLuaScreen() { return new LuaScreen(); }

// ======================== Apps Screen (sửa lỗi Lua_dofile) ========================
class AppsScreen : public Screen {
  String apps[20];
  int appCount;
  int selected;
  void scanApps() {
    appCount = 0;
    if (!SD.exists("/apps")) SD.mkdir("/apps");
    File dir = SD.open("/apps");
    while (File f = dir.openNextFile()) {
      if (String(f.name()).endsWith(".lua")) apps[appCount++] = f.name();
      f.close();
    }
    dir.close();
  }
  void runLuaFile(const char* path) {
    if (!SD.exists(path)) return;
    File f = SD.open(path);
    String script = "";
    while (f.available()) script += (char)f.read();
    f.close();
    lua.Lua_dostring(&script);
  }
public:
  void init() override { scanApps(); draw(); }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    for (int i=0; i<appCount; i++) {
      tft.setTextColor((i==selected)?TFT_CYAN:TFT_WHITE);
      tft.setCursor(10, 30 + i*20);
      tft.println(apps[i]);
    }
    if (appCount == 0) tft.println("No .lua apps in /apps");
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_UP) { selected = (selected-1+appCount)%appCount; draw(); }
    else if (btn == BTN_DOWN) { selected = (selected+1)%appCount; draw(); }
    else if (btn == BTN_START) {
      String path = "/apps/" + apps[selected];
      runLuaFile(path.c_str());
      tft.println("Executed " + apps[selected]);
      delay(1000);
      draw();
    } else if (btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createAppsScreen() { return new AppsScreen(); }

// ======================== Blender (OBJ Viewer) ========================
struct Vec3 { float x,y,z; };
class BlenderScreen : public Screen {
  std::vector<Vec3> vertices;
  std::vector<std::tuple<int,int,int>> faces;
  float rotX = 0, rotY = 0;
  float scale = 100;
  void loadOBJ(const char* filename) {
    vertices.clear(); faces.clear();
    File f = SD.open(filename);
    if (!f) return;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.startsWith("v ")) {
        Vec3 v;
        sscanf(line.c_str(), "v %f %f %f", &v.x, &v.y, &v.z);
        vertices.push_back(v);
      } else if (line.startsWith("f ")) {
        int a,b,c;
        sscanf(line.c_str(), "f %d %d %d", &a, &b, &c);
        faces.push_back({a-1,b-1,c-1});
      }
    }
    f.close();
  }
  void drawModel() {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    for (auto& face : faces) {
      Vec3 v1 = vertices[std::get<0>(face)];
      Vec3 v2 = vertices[std::get<1>(face)];
      Vec3 v3 = vertices[std::get<2>(face)];
      auto rot = [&](Vec3 v) {
        float x=v.x, y=v.y, z=v.z;
        float ry = y*cos(rotX) - z*sin(rotX);
        float rz = y*sin(rotX) + z*cos(rotX);
        float rx = x*cos(rotY) + rz*sin(rotY);
        float rz2 = -x*sin(rotY) + rz*cos(rotY);
        return Vec3{rx, ry, rz2};
      };
      Vec3 p1=rot(v1), p2=rot(v2), p3=rot(v3);
      int sx1 = 120 + p1.x*scale;
      int sy1 = 160 + p1.y*scale;
      int sx2 = 120 + p2.x*scale;
      int sy2 = 160 + p2.y*scale;
      int sx3 = 120 + p3.x*scale;
      int sy3 = 160 + p3.y*scale;
      tft.drawLine(sx1,sy1,sx2,sy2,TFT_WHITE);
      tft.drawLine(sx2,sy2,sx3,sy3,TFT_WHITE);
      tft.drawLine(sx3,sy3,sx1,sy1,TFT_WHITE);
    }
  }
public:
  void init() override { loadOBJ("/model.obj"); drawModel(); }
  void draw() override { drawModel(); }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_UP) rotX += 0.1;
    else if (btn == BTN_DOWN) rotX -= 0.1;
    else if (btn == BTN_LEFT) rotY -= 0.1;
    else if (btn == BTN_RIGHT) rotY += 0.1;
    else if (btn == BTN_MENU) setScreen(createMainMenu());
    drawModel();
  }
  void update() override {}
};
Screen* createBlenderScreen() { return new BlenderScreen(); }

// ======================== Settings Screen (sửa LEDC) ========================
class SettingsScreen : public Screen {
  int brightness = 128;
public:
  void init() override {
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, brightness);
    draw();
  }
  void draw() override {
    tft.fillScreen(TFT_BLACK);
    drawStatusBar();
    tft.setCursor(2,30);
    tft.printf("Brightness: %d\n\nA: Increase   B: Decrease", brightness);
  }
  void handleKey(uint8_t btn, bool pressed) override {
    if (!pressed) return;
    if (btn == BTN_A) { brightness = min(255, brightness+16); ledcWrite(0, brightness); draw(); }
    else if (btn == BTN_B) { brightness = max(0, brightness-16); ledcWrite(0, brightness); draw(); }
    else if (btn == BTN_MENU) setScreen(createMainMenu());
  }
  void update() override {}
};
Screen* createSettingsScreen() { return new SettingsScreen(); }

// ======================== Main Menu ========================
Screen* createMainMenu() {
  static MenuScreen::MenuItem items[] = {
    {"WiFi", createWiFiScreen},
    {"BLE", createBLEScreen},
    {"Media", createMediaScreen},
    {"File Manager", createFileManagerScreen},
    {"Web Browser", createWebBrowserScreen},
    {"GPIO", createGPIOScreen},
    {"Lua", createLuaScreen},
    {"Apps", createAppsScreen},
    {"Blender", createBlenderScreen},
    {"Settings", createSettingsScreen}
  };
  return new MenuScreen("ModBoxOS", items, 10);
}

// ======================== setup & loop ========================
void setup() {
  Serial.begin(115200);
  
  pinMode(TFT_BL, OUTPUT);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 128);
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  
  for (int i=0; i<10; i++) {
    pinMode(btnPins[i], INPUT_PULLUP);
    lastBtnState[i] = HIGH;
  }
  
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    tft.println("SD Card mount failed!");
    delay(2000);
  } else {
    tft.println("SD Card OK");
    delay(1000);
  }
  
  setScreen(createMainMenu());
}

void loop() {
  bool btnStates[10] = {false};
  readButtons(btnStates);
  if (currentScreen) currentScreen->update();
  delay(20);
}