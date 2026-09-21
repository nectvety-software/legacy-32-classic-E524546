/*
 * NokiaOS - Hệ điều hành cho ESP32-S3
 * Màn hình: TFT 2.4" 240x320 ST7789
 * Điều khiển: 10 nút bấm
 * 
 * Tác giả: AI Assistant
 * Version: 1.0.0
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <SPI.h>
#include <vector>
#include <math.h>

// ==================== ĐỊNH NGHĨA CHÂN ====================
#define KEY_UP 7
#define KEY_DOWN 46
#define KEY_LEFT 45
#define KEY_RIGHT 6
#define KEY_MENU 18
#define KEY_OPTION 8
#define KEY_SELECT 16
#define KEY_START 17
#define KEY_A 15
#define KEY_B 5

#define SD_CS 10
#define SD_MOSI 11
#define SD_SCLK 13
#define SD_MISO 9

// ==================== MÀU SẮC ====================
#define NOKIA_BLUE 0x001F
#define NOKIA_DARK_BLUE 0x0010
#define NOKIA_WHITE 0xFFFF
#define NOKIA_BLACK 0x0000
#define NOKIA_GRAY 0x8410
#define NOKIA_LIGHT_BLUE 0x06BF

#define MAX_MENU_ITEMS 20
#define MENU_ITEM_HEIGHT 24
#define VISIBLE_ITEMS 10

// ==================== ENUM ====================
enum AppType {
    APP_FILEMANAGER,
    APP_WIFI,
    APP_BLE,
    APP_MEDIA,
    APP_BROWSER,
    APP_GPIO,
    APP_LUA,
    APP_3DVIEWER,
    APP_SETTINGS
};

enum SystemState {
    STATE_BOOT,
    STATE_MENU,
    STATE_FILE_BROWSER,
    STATE_3D_VIEWER,
    STATE_WIFI,
    STATE_BLE,
    STATE_SETTINGS
};

// ==================== CẤU TRÚC DỮ LIỆU ====================
struct MenuItem {
  String name;
  AppType type;
};

struct FileInfo {
  String name;
  bool isDirectory;
  size_t size;
};

struct Vector3 {
  float x, y, z;
};

struct Face {
  int v[3];
  int n[3];
};

// ==================== BIẾN TOÀN CỤC ====================
TFT_eSPI tft = TFT_eSPI();
SystemState currentState = STATE_BOOT;
int currentMenuIndex = 0;
int menuScrollOffset = 0;
MenuItem menuItems[MAX_MENU_ITEMS];
int menuItemCount = 0;

// File Manager
std::vector<FileInfo> files;
int fileIndex = 0;
int fileScrollOffset = 0;
String currentPath = "/";

// 3D Viewer
std::vector<Vector3> vertices;
std::vector<Vector3> normals;
std::vector<Face> faces;
float rotX = 0, rotY = 0, rotZ = 0;
float objScale = 1.0;
float offsetX = 0, offsetY = 0, offsetZ = -5;
bool objLoaded = false;

// WiFi
bool wifiConnected = false;
std::vector<String> wifiNetworks;
int wifiIndex = 0;

// BLE
bool bleInitialized = false;
std::vector<String> bleDevices;
int bleIndex = 0;

// Settings
int settingsIndex = 0;
const char* settingsOptions[] = {"Brightness", "WiFi Settings", "About", "Restart"};
const int settingsCount = 4;

// ==================== KHAI BÁO HÀM ====================
void setup();
void loop();

// System Functions
void initSystem();
void bootSequence();
void drawMenu();
void drawMenuItem(int index, bool selected);
void drawHeader(const String& title);
void drawStatusBar();
void showMessage(const String& msg, int duration = 2000);
void navigateMenu(int direction);
void selectMenuItem();
void goBack();

// File Manager
void openFileManager();
void loadDirectory(const String& path);
void drawFileList();
void drawFileItem(int index, bool selected);
void fileManagerLoop();
void fileNavigate(int direction);
void fileSelect();

// 3D Viewer
void open3DViewer();
bool loadOBJFile(const String& filename);
void parseOBJFile(File& file);
void render3D();
void projectVertex(const Vector3& v, int& x, int& y);
Vector3 rotateVertex(const Vector3& v);
void draw3DTriangle(const Vector3& v1, const Vector3& v2, const Vector3& v3, uint16_t color);
uint16_t getColorFromNormal(const Vector3& normal);
void viewer3DLoop();

// WiFi Manager
void openWiFiManager();
void scanWiFi();
void drawWiFiList();
void wifiManagerLoop();

// BLE Manager
void openBLEManager();
void scanBLE();
void drawBLEList();
void bleManagerLoop();

// Settings
void openSettings();
void drawSettings();
void settingsLoop();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== NokiaOS Starting ===");
  
  // Cấu hình nút bấm
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_LEFT, INPUT_PULLUP);
  pinMode(KEY_RIGHT, INPUT_PULLUP);
  pinMode(KEY_MENU, INPUT_PULLUP);
  pinMode(KEY_OPTION, INPUT_PULLUP);
  pinMode(KEY_SELECT, INPUT_PULLUP);
  pinMode(KEY_START, INPUT_PULLUP);
  pinMode(KEY_A, INPUT_PULLUP);
  pinMode(KEY_B, INPUT_PULLUP);
  
  initSystem();
}

// ==================== LOOP ====================
void loop() {
  switch (currentState) {
    case STATE_MENU:
      handleMenuInput();
      break;
    case STATE_FILE_BROWSER:
      fileManagerLoop();
      break;
    case STATE_3D_VIEWER:
      viewer3DLoop();
      break;
    case STATE_WIFI:
      wifiManagerLoop();
      break;
    case STATE_BLE:
      bleManagerLoop();
      break;
    case STATE_SETTINGS:
      settingsLoop();
      break;
  }
}

// ==================== XỬ LÝ INPUT MENU ====================
void handleMenuInput() {
  if (digitalRead(KEY_UP) == LOW) {
    delay(50);
    if (digitalRead(KEY_UP) == LOW) {
      navigateMenu(-1);
      while (digitalRead(KEY_UP) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_DOWN) == LOW) {
    delay(50);
    if (digitalRead(KEY_DOWN) == LOW) {
      navigateMenu(1);
      while (digitalRead(KEY_DOWN) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_START) == LOW) {
    delay(50);
    if (digitalRead(KEY_START) == LOW) {
      selectMenuItem();
      while (digitalRead(KEY_START) == LOW) delay(10);
    }
  }
}

// ==================== HỆ THỐNG ====================
void initSystem() {
  Serial.println("Initializing TFT...");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(NOKIA_BLACK);
  tft.setTextColor(NOKIA_WHITE, NOKIA_BLACK);
  tft.setTextSize(2);
  
  // LED Backlight - Sửa cho ESP32 Core 3.x
  Serial.println("Initializing LED backlight...");
  ledcAttach(39, 5000, 8);  // Pin 39, frequency 5000Hz, 8-bit resolution
  ledcWrite(39, 255);        // Set brightness to max
  
  // SD Card
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS, SPI, 80000000)) {
    Serial.println("SD Card initialization failed!");
    showMessage("SD Card Error!");
    delay(2000);
  } else {
    Serial.println("SD Card initialized.");
  }
  
  // WiFi
  WiFi.mode(WIFI_STA);
  
  // BLE
  Serial.println("Initializing BLE...");
  BLEDevice::init("NokiaOS-ESP32");
  bleInitialized = true;
  
  bootSequence();
  
  // Thêm menu items
  Serial.println("Loading menu items...");
  menuItems[menuItemCount++] = {"File Manager", APP_FILEMANAGER};
  menuItems[menuItemCount++] = {"WiFi", APP_WIFI};
  menuItems[menuItemCount++] = {"Bluetooth", APP_BLE};
  menuItems[menuItemCount++] = {"Media", APP_MEDIA};
  menuItems[menuItemCount++] = {"3D Viewer", APP_3DVIEWER};
  menuItems[menuItemCount++] = {"Settings", APP_SETTINGS};
  
  currentState = STATE_MENU;
  drawMenu();
  Serial.println("NokiaOS Ready!");
}

void bootSequence() {
  tft.fillScreen(NOKIA_BLUE);
  tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("NokiaOS", tft.width()/2, tft.height()/2 - 20, 7);
  tft.setTextSize(1);
  tft.drawString("ESP32-S3 Edition", tft.width()/2, tft.height()/2 + 10, 4);
  tft.drawString("v1.0.0", tft.width()/2, tft.height()/2 + 30, 2);
  delay(2000);
  tft.setTextDatum(TL_DATUM);
}

void drawMenu() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("Menu");
  
  int visibleCount = min(menuItemCount, VISIBLE_ITEMS);
  for (int i = 0; i < visibleCount; i++) {
    int actualIndex = i + menuScrollOffset;
    if (actualIndex < menuItemCount) {
      drawMenuItem(actualIndex, actualIndex == currentMenuIndex);
    }
  }
  drawStatusBar();
}

void drawMenuItem(int index, bool selected) {
  int y = 30 + (index - menuScrollOffset) * MENU_ITEM_HEIGHT;
  
  if (selected) {
    tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLUE);
    tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
  } else {
    tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLACK);
    tft.setTextColor(NOKIA_WHITE, NOKIA_BLACK);
  }
  
  tft.setTextSize(2);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(">", 5, y + 12, 4);
  tft.drawString(menuItems[index].name, 20, y + 12, 4);
}

void drawHeader(const String& title) {
  tft.fillRect(0, 0, tft.width(), 28, NOKIA_BLUE);
  tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title, tft.width()/2, 8, 4);
  tft.setTextDatum(TL_DATUM);
}

void drawStatusBar() {
  tft.fillRect(0, tft.height() - 20, tft.width(), 20, NOKIA_DARK_BLUE);
  tft.setTextColor(NOKIA_WHITE, NOKIA_DARK_BLUE);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  String status = String(millis()/1000/60) + "m";
  if (wifiConnected) status += " WiFi";
  tft.drawString(status, tft.width()/2, tft.height() - 12, 2);
  tft.setTextDatum(TL_DATUM);
}

void showMessage(const String& msg, int duration) {
  int w = tft.width();
  int h = tft.height();
  tft.fillRect(w/2 - 100, h/2 - 20, 200, 40, NOKIA_BLUE);
  tft.drawRect(w/2 - 100, h/2 - 20, 200, 40, NOKIA_WHITE);
  tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, w/2, h/2, 4);
  tft.setTextDatum(TL_DATUM);
  if (duration > 0) {
    delay(duration);
  }
}

void navigateMenu(int direction) {
  currentMenuIndex += direction;
  if (currentMenuIndex < 0) currentMenuIndex = menuItemCount - 1;
  if (currentMenuIndex >= menuItemCount) currentMenuIndex = 0;
  
  if (currentMenuIndex < menuScrollOffset) {
    menuScrollOffset = currentMenuIndex;
  } else if (currentMenuIndex >= menuScrollOffset + VISIBLE_ITEMS) {
    menuScrollOffset = currentMenuIndex - VISIBLE_ITEMS + 1;
  }
  drawMenu();
}

void selectMenuItem() {
  switch (menuItems[currentMenuIndex].type) {
    case APP_FILEMANAGER:
      openFileManager();
      break;
    case APP_WIFI:
      openWiFiManager();
      break;
    case APP_BLE:
      openBLEManager();
      break;
    case APP_3DVIEWER:
      open3DViewer();
      break;
    case APP_SETTINGS:
      openSettings();
      break;
    default:
      showMessage("Coming soon...");
  }
}

void goBack() {
  currentState = STATE_MENU;
  drawMenu();
}

// ==================== FILE MANAGER ====================
void openFileManager() {
  currentPath = "/";
  loadDirectory(currentPath);
  currentState = STATE_FILE_BROWSER;
}

void loadDirectory(const String& path) {
  files.clear();
  fileIndex = 0;
  fileScrollOffset = 0;
  
  File root = SD.open(path.c_str());
  if (!root) {
    showMessage("Cannot open dir");
    return;
  }
  
  // Add parent directory if not root
  if (path != "/") {
    FileInfo info;
    info.name = "..";
    info.isDirectory = true;
    info.size = 0;
    files.push_back(info);
  }
  
  // Read files and directories
  File file = root.openNextFile();
  while (file) {
    FileInfo info;
    info.name = file.name();
    info.isDirectory = file.isDirectory();
    info.size = file.size();
    files.push_back(info);
    file = root.openNextFile();
  }
  root.close();
  
  drawFileList();
}

void drawFileList() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("Files: " + currentPath);
  
  int visibleCount = min((int)files.size(), VISIBLE_ITEMS);
  for (int i = 0; i < visibleCount; i++) {
    int actualIndex = i + fileScrollOffset;
    if (actualIndex < (int)files.size()) {
      drawFileItem(actualIndex, actualIndex == fileIndex);
    }
  }
}

void drawFileItem(int index, bool selected) {
  int y = 30 + (index - fileScrollOffset) * MENU_ITEM_HEIGHT;
  
  if (selected) {
    tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLUE);
    tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
  } else {
    tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLACK);
    tft.setTextColor(NOKIA_WHITE, NOKIA_BLACK);
  }
  
  tft.setTextSize(2);
  tft.setTextDatum(ML_DATUM);
  
  String icon = files[index].isDirectory ? ">" : "#";
  tft.drawString(icon, 5, y + 12, 4);
  
  String name = files[index].name;
  if (name.length() > 18) name = name.substring(0, 15) + "...";
  tft.drawString(name, 20, y + 12, 4);
  
  if (!files[index].isDirectory) {
    tft.setTextSize(1);
    String sizeStr = String(files[index].size / 1024) + "KB";
    tft.drawString(sizeStr, tft.width() - 5, y + 12, 2);
  }
}

void fileManagerLoop() {
  if (digitalRead(KEY_UP) == LOW) {
    delay(50);
    if (digitalRead(KEY_UP) == LOW) {
      fileNavigate(-1);
      while (digitalRead(KEY_UP) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_DOWN) == LOW) {
    delay(50);
    if (digitalRead(KEY_DOWN) == LOW) {
      fileNavigate(1);
      while (digitalRead(KEY_DOWN) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_START) == LOW) {
    delay(50);
    if (digitalRead(KEY_START) == LOW) {
      fileSelect();
      while (digitalRead(KEY_START) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_A) == LOW) {
    delay(50);
    if (digitalRead(KEY_A) == LOW) {
      goBack();
      while (digitalRead(KEY_A) == LOW) delay(10);
    }
  }
}

void fileNavigate(int direction) {
  fileIndex += direction;
  if (fileIndex < 0) fileIndex = files.size() - 1;
  if (fileIndex >= (int)files.size()) fileIndex = 0;
  
  if (fileIndex < fileScrollOffset) {
    fileScrollOffset = fileIndex;
  } else if (fileIndex >= fileScrollOffset + VISIBLE_ITEMS) {
    fileScrollOffset = fileIndex - VISIBLE_ITEMS + 1;
  }
  drawFileList();
}

void fileSelect() {
  if (fileIndex >= (int)files.size()) return;
  
  FileInfo& file = files[fileIndex];
  
  if (file.name == "..") {
    // Go to parent directory
    int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
    if (lastSlash >= 0) {
      currentPath = currentPath.substring(0, lastSlash + 1);
    } else {
      currentPath = "/";
    }
    loadDirectory(currentPath);
  } else if (file.isDirectory) {
    // Enter directory
    if (currentPath == "/") {
      currentPath += file.name;
    } else {
      currentPath += "/" + file.name;
    }
    loadDirectory(currentPath);
  } else {
    // Open file
    if (file.name.endsWith(".obj")) {
      if (loadOBJFile(currentPath + "/" + file.name)) {
        currentState = STATE_3D_VIEWER;
      }
    } else {
      showMessage("Unsupported file");
    }
  }
}

// ==================== 3D VIEWER ====================
void open3DViewer() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("3D Viewer");
  showMessage("Select .obj file", 1500);
  openFileManager();
}

bool loadOBJFile(const String& filename) {
  Serial.println("Loading OBJ: " + filename);
  vertices.clear();
  normals.clear();
  faces.clear();
  
  File file = SD.open(filename.c_str());
  if (!file) {
    showMessage("Cannot open file");
    return false;
  }
  
  parseOBJFile(file);
  file.close();
  
  if (vertices.size() > 0 && faces.size() > 0) {
    objLoaded = true;
    rotX = 0;
    rotY = 0;
    rotZ = 0;
    objScale = 1.0;
    tft.fillScreen(NOKIA_BLACK);
    drawHeader("3D: " + filename);
    render3D();
    showMessage(String(faces.size()) + " faces", 1000);
    return true;
  }
  return false;
}

void parseOBJFile(File& file) {
  int lineCount = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    lineCount++;
    
    if (line.startsWith("v ")) {
      // Vertex
      Vector3 v;
      if (sscanf(line.c_str(), "v %f %f %f", &v.x, &v.y, &v.z) == 3) {
        vertices.push_back(v);
      }
    } else if (line.startsWith("vn ")) {
      // Normal
      Vector3 n;
      if (sscanf(line.c_str(), "vn %f %f %f", &n.x, &n.y, &n.z) == 3) {
        normals.push_back(n);
      }
    } else if (line.startsWith("f ")) {
      // Face
      Face f;
      int v1, v2, v3, n1, n2, n3;
      if (sscanf(line.c_str(), "f %d//%d %d//%d %d//%d", 
                &v1, &n1, &v2, &n2, &v3, &n3) == 6) {
        f.v[0] = v1 - 1;
        f.v[1] = v2 - 1;
        f.v[2] = v3 - 1;
        f.n[0] = n1 > 0 ? n1 - 1 : -1;
        f.n[1] = n2 > 0 ? n2 - 1 : -1;
        f.n[2] = n3 > 0 ? n3 - 1 : -1;
        faces.push_back(f);
      }
    }
    
    if (lineCount % 100 == 0) {
      delay(1); // Avoid watchdog reset
    }
  }
  
  Serial.println("Parsed: " + String(vertices.size()) + " vertices, " + 
                 String(faces.size()) + " faces");
}

void render3D() {
  if (!objLoaded) return;
  
  tft.fillRect(0, 28, tft.width(), tft.height() - 48, NOKIA_BLACK);
  
  for (const auto& face : faces) {
    Vector3 v1 = rotateVertex(vertices[face.v[0]]);
    Vector3 v2 = rotateVertex(vertices[face.v[1]]);
    Vector3 v3 = rotateVertex(vertices[face.v[2]]);
    
    // Calculate normal for lighting
    Vector3 edge1 = {v2.x - v1.x, v2.y - v1.y, v2.z - v1.z};
    Vector3 edge2 = {v3.x - v1.x, v3.y - v1.y, v3.z - v1.z};
    Vector3 normal = {
      edge1.y * edge2.z - edge1.z * edge2.y,
      edge1.z * edge2.x - edge1.x * edge2.z,
      edge1.x * edge2.y - edge1.y * edge2.x
    };
    
    // Normalize
    float len = sqrt(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
    if (len > 0) {
      normal.x /= len;
      normal.y /= len;
      normal.z /= len;
    }
    
    uint16_t color = getColorFromNormal(normal);
    draw3DTriangle(v1, v2, v3, color);
  }
}

Vector3 rotateVertex(const Vector3& v) {
  Vector3 r = v;
  
  // Rotate X
  float y = r.y * cos(rotX) - r.z * sin(rotX);
  float z = r.y * sin(rotX) + r.z * cos(rotX);
  r.y = y;
  r.z = z;
  
  // Rotate Y
  float x = r.x * cos(rotY) + r.z * sin(rotY);
  z = -r.x * sin(rotY) + r.z * cos(rotY);
  r.x = x;
  r.z = z;
  
  return r;
}

void projectVertex(const Vector3& v, int& x, int& y) {
  float fov = 256.0;
  float scale_proj = fov / (5.0 + v.z + offsetZ);
  x = (int)(v.x * objScale * scale_proj) + tft.width()/2 + (int)offsetX;
  y = (int)(v.y * objScale * scale_proj) + tft.height()/2 + (int)offsetY;
}

uint16_t getColorFromNormal(const Vector3& normal) {
  // Simple lighting based on normal Z
  float intensity = normal.z * 0.5 + 0.5;
  if (intensity < 0) intensity = 0;
  if (intensity > 1) intensity = 1;
  
  uint8_t r = (uint8_t)(intensity * 31);
  uint8_t g = (uint8_t)(intensity * 63);
  uint8_t b = (uint8_t)(intensity * 31);
  
  return tft.color565(r * 8, g * 4, b * 8);
}

void draw3DTriangle(const Vector3& v1, const Vector3& v2, const Vector3& v3, uint16_t color) {
  int x1, y1, x2, y2, x3, y3;
  projectVertex(v1, x1, y1);
  projectVertex(v2, x2, y2);
  projectVertex(v3, x3, y3);
  
  // Backface culling
  int cross = (x2-x1)*(y3-y1) - (y2-y1)*(x3-x1);
  if (cross < 0) return;
  
  tft.fillTriangle(x1, y1, x2, y2, x3, y3, color);
}

void viewer3DLoop() {
  if (digitalRead(KEY_UP) == LOW) {
    delay(50);
    if (digitalRead(KEY_UP) == LOW) {
      rotX += 0.1;
      render3D();
      while (digitalRead(KEY_UP) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_DOWN) == LOW) {
    delay(50);
    if (digitalRead(KEY_DOWN) == LOW) {
      rotX -= 0.1;
      render3D();
      while (digitalRead(KEY_DOWN) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_LEFT) == LOW) {
    delay(50);
    if (digitalRead(KEY_LEFT) == LOW) {
      rotY -= 0.1;
      render3D();
      while (digitalRead(KEY_LEFT) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_RIGHT) == LOW) {
    delay(50);
    if (digitalRead(KEY_RIGHT) == LOW) {
      rotY += 0.1;
      render3D();
      while (digitalRead(KEY_RIGHT) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_START) == LOW) {
    delay(50);
    if (digitalRead(KEY_START) == LOW) {
      objScale *= 1.1;
      render3D();
      while (digitalRead(KEY_START) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_B) == LOW) {
    delay(50);
    if (digitalRead(KEY_B) == LOW) {
      objScale *= 0.9;
      render3D();
      while (digitalRead(KEY_B) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_MENU) == LOW) {
    delay(50);
    if (digitalRead(KEY_MENU) == LOW) {
      goBack();
      while (digitalRead(KEY_MENU) == LOW) delay(10);
    }
  }
}

// ==================== WIFI MANAGER ====================
void openWiFiManager() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("WiFi");
  scanWiFi();
  currentState = STATE_WIFI;
}

void scanWiFi() {
  showMessage("Scanning...");
  wifiNetworks.clear();
  
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    wifiNetworks.push_back(WiFi.SSID(i));
  }
  wifiIndex = 0;
  drawWiFiList();
}

void drawWiFiList() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("WiFi Networks");
  
  int visibleCount = min((int)wifiNetworks.size(), VISIBLE_ITEMS);
  for (int i = 0; i < visibleCount; i++) {
    int y = 30 + i * MENU_ITEM_HEIGHT;
    bool selected = (i == wifiIndex);
    
    if (selected) {
      tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLUE);
      tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
    } else {
      tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLACK);
      tft.setTextColor(NOKIA_WHITE, NOKIA_BLACK);
    }
    
    tft.setTextSize(2);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(">", 5, y + 12, 4);
    
    String name = wifiNetworks[i];
    if (name.length() > 20) name = name.substring(0, 17) + "...";
    tft.drawString(name, 20, y + 12, 4);
  }
}

void wifiManagerLoop() {
  if (digitalRead(KEY_UP) == LOW) {
    delay(50);
    if (digitalRead(KEY_UP) == LOW) {
      wifiIndex--;
      if (wifiIndex < 0) wifiIndex = wifiNetworks.size() - 1;
      drawWiFiList();
      while (digitalRead(KEY_UP) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_DOWN) == LOW) {
    delay(50);
    if (digitalRead(KEY_DOWN) == LOW) {
      wifiIndex++;
      if (wifiIndex >= (int)wifiNetworks.size()) wifiIndex = 0;
      drawWiFiList();
      while (digitalRead(KEY_DOWN) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_START) == LOW) {
    delay(50);
    if (digitalRead(KEY_START) == LOW) {
      showMessage("Enter password");
      while (digitalRead(KEY_START) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_A) == LOW) {
    delay(50);
    if (digitalRead(KEY_A) == LOW) {
      goBack();
      while (digitalRead(KEY_A) == LOW) delay(10);
    }
  }
}

// ==================== BLE MANAGER ====================
void openBLEManager() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("Bluetooth");
  scanBLE();
  currentState = STATE_BLE;
}

void scanBLE() {
  showMessage("Scanning BLE...");
  bleDevices.clear();
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  
  // ESP32 Core 3.x: start() trả về pointer
  BLEScanResults* foundDevices = pBLEScan->start(5, false);
  
  if (foundDevices) {
    for (int i = 0; i < foundDevices->getCount(); i++) {
      BLEAdvertisedDevice device = foundDevices->getDevice(i);
      String name = device.getName().c_str();
      if (name.length() == 0) name = "Unknown";
      bleDevices.push_back(name);
    }
  }
  
  pBLEScan->stop();
  bleIndex = 0;
  drawBLEList();
}

void drawBLEList() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("BLE Devices");
  
  int visibleCount = min((int)bleDevices.size(), VISIBLE_ITEMS);
  for (int i = 0; i < visibleCount; i++) {
    int y = 30 + i * MENU_ITEM_HEIGHT;
    bool selected = (i == bleIndex);
    
    if (selected) {
      tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLUE);
      tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
    } else {
      tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLACK);
      tft.setTextColor(NOKIA_WHITE, NOKIA_BLACK);
    }
    
    tft.setTextSize(2);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(">", 5, y + 12, 4);
    tft.drawString(bleDevices[i], 20, y + 12, 4);
  }
}

void bleManagerLoop() {
  if (digitalRead(KEY_UP) == LOW) {
    delay(50);
    if (digitalRead(KEY_UP) == LOW) {
      bleIndex--;
      if (bleIndex < 0) bleIndex = bleDevices.size() - 1;
      drawBLEList();
      while (digitalRead(KEY_UP) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_DOWN) == LOW) {
    delay(50);
    if (digitalRead(KEY_DOWN) == LOW) {
      bleIndex++;
      if (bleIndex >= (int)bleDevices.size()) bleIndex = 0;
      drawBLEList();
      while (digitalRead(KEY_DOWN) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_START) == LOW) {
    delay(50);
    if (digitalRead(KEY_START) == LOW) {
      showMessage("Connecting...");
      while (digitalRead(KEY_START) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_A) == LOW) {
    delay(50);
    if (digitalRead(KEY_A) == LOW) {
      goBack();
      while (digitalRead(KEY_A) == LOW) delay(10);
    }
  }
}

// ==================== SETTINGS ====================
void openSettings() {
  drawSettings();
  currentState = STATE_SETTINGS;
}

void drawSettings() {
  tft.fillScreen(NOKIA_BLACK);
  drawHeader("Settings");
  
  for (int i = 0; i < settingsCount; i++) {
    int y = 30 + i * MENU_ITEM_HEIGHT;
    bool selected = (i == settingsIndex);
    
    if (selected) {
      tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLUE);
      tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
    } else {
      tft.fillRect(0, y, tft.width(), MENU_ITEM_HEIGHT, NOKIA_BLACK);
      tft.setTextColor(NOKIA_WHITE, NOKIA_BLACK);
    }
    
    tft.setTextSize(2);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(">", 5, y + 12, 4);
    tft.drawString(settingsOptions[i], 20, y + 12, 4);
  }
}

void settingsLoop() {
  if (digitalRead(KEY_UP) == LOW) {
    delay(50);
    if (digitalRead(KEY_UP) == LOW) {
      settingsIndex--;
      if (settingsIndex < 0) settingsIndex = settingsCount - 1;
      drawSettings();
      while (digitalRead(KEY_UP) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_DOWN) == LOW) {
    delay(50);
    if (digitalRead(KEY_DOWN) == LOW) {
      settingsIndex++;
      if (settingsIndex >= settingsCount) settingsIndex = 0;
      drawSettings();
      while (digitalRead(KEY_DOWN) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_START) == LOW) {
    delay(50);
    if (digitalRead(KEY_START) == LOW) {
      switch (settingsIndex) {
        case 0:
          showMessage("Brightness: 100%");
          break;
        case 1:
          showMessage("WiFi Config");
          break;
        case 2:
          showMessage("NokiaOS v1.0");
          break;
        case 3:
          showMessage("Restarting...");
          delay(1000);
          ESP.restart();
          break;
      }
      while (digitalRead(KEY_START) == LOW) delay(10);
    }
  }
  
  if (digitalRead(KEY_A) == LOW) {
    delay(50);
    if (digitalRead(KEY_A) == LOW) {
      goBack();
      while (digitalRead(KEY_A) == LOW) delay(10);
    }
  }
}