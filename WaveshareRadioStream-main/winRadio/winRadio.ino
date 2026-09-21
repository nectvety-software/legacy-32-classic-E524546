#pragma GCC optimize("Os")

#include "Arduino.h"
#include "WiFiMulti.h"
#include "Audio.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Preferences.h>

#define TFT_CS 14
#define TFT_RST 3
#define TFT_DC 47
#define TFT_MOSI 12
#define TFT_SCK 48
#define GFX_BL 39

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

#define BLACK 0x0000
#define YELLOW 0xFFE0
#define ORANGE 0xFA80
#define TFT_RED 0xF800
#define TFT_GREEN 0x07E0
#define TFT_BLACK 0x0000
#define TFT_DARKGREEN 0x03E0
#define TFT_WHITE 0xFFFF

#define I2S_BCLK 9
#define I2S_DOUT 2
#define I2S_LRC 10
// I2S_MCLK doi sang 38: GPIO8 la KEY_OPTION (keypad Symbian), xung MCLK
// tren chan phim gay nhan phim ao.
#define I2S_MCLK 38

// Button pins
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

Preferences prefs;

String curStation = "";
String songPlaying = "";
long bitrate = 0;
bool connected = false;

Audio audio;
WiFiMulti wifiMulti;
String ssid = "";
String password = "";

bool canDraw = false;
bool drawDone = false;
bool btnPressed[10] = {false};
int rssi = 0;
int chosen = 0;
int volume = 2;
unsigned long lastBtnTime = 0;
#define DEBOUNCE_MS 150

bool forceRedraw = true;
int lastChosen = -1;
int lastVolume = -1;
int lastRSSI = 0;
long lastBitrate = 0;
String lastSongPlaying = "";

int mode = 0; // 0=radio, 1=WiFi Menu, 10=Scan, 11=Saved, 12=Options, 13=Keyboard

int kbdRow = 0;
int kbdCol = 0;
int inputField = 0; // 0 = ssid, 1 = password
bool isCaps = true;
const char* kbdChars[] = {
  "1234567890",
  "ABCDEFGHIJ",
  "KLMNOPQRST",
  "UVWXYZ-/. "
};
const char* kbdCharsLow[] = {
  "1234567890",
  "abcdefghij",
  "klmnopqrst",
  "uvwxyz-/. "
};

#define ns 6
const char* stations[ns] = {
  "1.Discodiamond", "2.Radioking", "3.Radiocaroline", "4.Rautemusik", "5.WGMC", "6.Banovina"
};
const char* stationUrls[ns] = {
  "https://discodiamond.radioca.st/autodj",
  "https://listen.radioking.com/radio/175279/stream/216784",
  "http://sc6.radiocaroline.net:8040/stream",
  "https://club-high.rautemusik.fm/;",
  "http://greece-media.monroe.edu/wgmc.mp3",
  "https://audio.radio-banovina.hr:9998/;"
};

void initButtons() {
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
}

struct WiFiCredentials {
  char ssid[33];
  char pass[65];
};
#define MAX_SAVED 10
WiFiCredentials savedNets[MAX_SAVED];
int savedCount = 0;
int selectedNet = -1; // Index in scan or saved list
int scanCount = 0;

void loadWiFiCredentials() {
  prefs.begin("wifiCfg", false);
  savedCount = prefs.getInt("count", 0);
  if (savedCount > MAX_SAVED) savedCount = MAX_SAVED;
  prefs.getBytes("nets", savedNets, sizeof(savedNets));
  prefs.end();
  
  if (savedCount > 0) {
    ssid = String(savedNets[0].ssid);
    password = String(savedNets[0].pass);
  } else {
    ssid = "TOTOLINK_A720R";
    password = "0973951700";
  }
}

void saveWiFiCredentials() {
  // Check if current ssid already exists
  int idx = -1;
  for (int i = 0; i < savedCount; i++) {
    if (ssid == String(savedNets[i].ssid)) {
      idx = i;
      break;
    }
  }
  
  if (idx == -1 && savedCount < MAX_SAVED) {
    idx = savedCount++;
  }
  
  if (idx != -1) {
    strncpy(savedNets[idx].ssid, ssid.c_str(), 32);
    strncpy(savedNets[idx].pass, password.c_str(), 64);
    
    prefs.begin("wifiCfg", false);
    prefs.putInt("count", savedCount);
    prefs.putBytes("nets", savedNets, sizeof(savedNets));
    prefs.end();
  }
}

void forgetWiFi(int index) {
  if (index < 0 || index >= savedCount) return;
  for (int i = index; i < savedCount - 1; i++) {
    savedNets[i] = savedNets[i + 1];
  }
  savedCount--;
  prefs.begin("wifiCfg", false);
  prefs.putInt("count", savedCount);
  prefs.putBytes("nets", savedNets, sizeof(savedNets));
  prefs.end();
}

void drawKeyboard() {
  uint16_t gray = tft.color565(40, 40, 80);
  uint16_t selectColor = TFT_GREEN;
  uint16_t dark = tft.color565(20, 20, 40);
  uint16_t funcColor = tft.color565(60, 60, 100);

  // Keyboard background area
  tft.fillRect(0, 145, 240, 175, dark);
  
  // Character rows (0-3)
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 10; c++) {
      int x = 5 + c * 23;
      int y = 150 + r * 32;
      char ch = isCaps ? kbdChars[r][c] : kbdCharsLow[r][c];
      
      if (r == kbdRow && c == kbdCol) {
        tft.fillRect(x, y, 20, 28, selectColor);
        tft.setTextColor(BLACK, selectColor);
      } else {
        tft.fillRect(x, y, 20, 28, gray);
        tft.setTextColor(TFT_WHITE, gray);
      }
      tft.setTextSize(1);
      tft.setCursor(x + 7, y + 10);
      tft.print(ch);
    }
  }
  
  // Row 4: Special keys [CAPS] [SPACE] [DEL] [BACK] [DONE]
  const char* funcKeys[] = {"CAPS", "SPACE", "DEL", "BACK", "DONE"};
  for (int r4 = 0; r4 < 5; r4++) {
    int x = 5 + r4 * 47;
    int y = 150 + 4 * 32;
    int w = 44;
    
    // Check selection: Row 4 mapping is simplified to 5 items (cols 0, 2, 4, 6, 8)
    bool isSelected = (kbdRow == 4 && (kbdCol / 2) == r4);
    
    if (isSelected) {
      tft.fillRect(x, y, w, 28, selectColor);
      tft.setTextColor(BLACK, selectColor);
    } else {
      tft.fillRect(x, y, w, 28, funcColor);
      tft.setTextColor(TFT_WHITE, funcColor);
    }
    tft.setTextSize(1);
    tft.setCursor(x + (w - strlen(funcKeys[r4]) * 6) / 2, y + 10);
    tft.print(funcKeys[r4]);
  }
}

void drawWiFiMenu() {
  uint16_t dark = tft.color565(20, 20, 40);
  uint16_t light = tft.color565(100, 100, 140);
  tft.fillScreen(dark);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 20);
  tft.print("WiFi Settings");

  const char* menu[] = {"1. Scan Networks", "2. Saved Networks", "3. Manual Input"};
  for (int i = 0; i < 3; i++) {
    if (i == chosen) {
      tft.fillRect(5, 60 + i * 40, 230, 35, TFT_GREEN);
      tft.setTextColor(BLACK);
    } else {
      tft.drawRect(5, 60 + i * 40, 230, 35, light);
      tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(15, 70 + i * 40);
    tft.print(menu[i]);
  }
}

void drawWiFiScan() {
  uint16_t dark = tft.color565(20, 20, 40);
  uint16_t light = tft.color565(100, 100, 140);
  uint16_t gray = tft.color565(40, 40, 80);
  tft.fillScreen(dark);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.setTextColor(light);
  tft.print("Found Networks (UP/DOWN to move):");

  for (int i = 0; i < scanCount && i < 8; i++) {
    int y = 30 + i * 35;
    if (i == chosen) {
      tft.fillRect(5, y, 230, 30, TFT_GREEN);
      tft.setTextColor(BLACK);
    } else {
      tft.drawRect(5, y, 230, 30, gray);
      tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(10, y + 5);
    tft.print(WiFi.SSID(i).substring(0, 20));
    tft.setCursor(10, y + 18);
    String sec = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Locked";
    tft.print(sec);
    tft.setCursor(180, y + 10);
    tft.print(WiFi.RSSI(i));
    tft.print("dBm");
  }
}

void drawKeyboardUI() {
  uint16_t dark = tft.color565(20, 20, 40);
  uint16_t light = tft.color565(100, 100, 140);
  tft.fillScreen(dark);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 10);
  tft.print("Enter ");
  tft.print(inputField == 0 ? "SSID" : "Password");
  
  tft.fillRect(10, 40, 220, 30, BLACK);
  tft.drawRect(10, 40, 220, 30, light);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(1);
  tft.setCursor(15, 50);
  if (inputField == 0) tft.print(ssid);
  else {
    String pStr = "";
    for(int i=0; i<password.length(); i++) pStr += "*";
    tft.print(pStr);
  }
  
  drawKeyboard();
}

void drawSavedNets() {
  uint16_t dark = tft.color565(20, 20, 40);
  tft.fillScreen(dark);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.setTextColor(TFT_WHITE);
  tft.print("Saved WiFi");

  if (savedCount == 0) {
    tft.setCursor(10, 100);
    tft.print("No saved nets");
    return;
  }

  for (int i = 0; i < savedCount; i++) {
    int y = 40 + i * 30;
    if (i == chosen) {
      tft.fillRect(5, y, 230, 25, TFT_GREEN);
      tft.setTextColor(BLACK);
    } else {
      tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(10, y + 5);
    tft.print(savedNets[i].ssid);
  }
}

void drawNetOptions() {
  uint16_t dark = tft.color565(20, 20, 40);
  tft.fillScreen(dark);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.setTextColor(TFT_WHITE);
  tft.print("Network Info:");
  tft.setCursor(10, 40);
  tft.print(ssid);

  const char* opts[] = {"Connect", "Edit Password", "Forget Network", "Back"};
  for (int i = 0; i < 4; i++) {
    int y = 80 + i * 40;
    if (i == chosen) {
      tft.fillRect(5, y, 230, 35, TFT_GREEN);
      tft.setTextColor(BLACK);
    } else {
      tft.drawRect(5, y, 230, 35, tft.color565(100, 100, 140));
      tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(15, y + 10);
    tft.print(opts[i]);
  }
}

void handleWiFiInput() {
  bool up = (digitalRead(KEY_UP) == LOW);
  bool down = (digitalRead(KEY_DOWN) == LOW);
  bool left = (digitalRead(KEY_LEFT) == LOW);
  bool right = (digitalRead(KEY_RIGHT) == LOW);
  bool a = (digitalRead(KEY_A) == LOW);
  bool b = (digitalRead(KEY_B) == LOW);
  bool select = (digitalRead(KEY_SELECT) == LOW);
  bool menu = (digitalRead(KEY_MENU) == LOW);
  bool start = (digitalRead(KEY_START) == LOW);  // OK giua (Symbian)

  static bool upL, downL, leftL, rightL, aL, bL, selectL, menuL, startL;

  // Debounce check
  bool anyPressed = (up || down || left || right || a || b || start || menu);
  if (anyPressed && (millis() - lastBtnTime < DEBOUNCE_MS)) return;

  bool pressOk = (start && !startL);  // OK giua chon muc (Symbian map moi)
  if (anyPressed && !upL && !downL && !leftL && !rightL && !aL && !bL && !startL && !menuL) {
    lastBtnTime = millis();
  }

  if (menu && !menuL) { mode = 0; forceRedraw = true; canDraw = true; }

  // Navigation Logic
  if (mode == 1 || mode == 10 || mode == 11 || mode == 12) {
    int maxItems = (mode == 1) ? 3 : (mode == 10) ? scanCount : (mode == 11) ? savedCount : 4;
    if (up && !upL) { chosen--; if (chosen < 0) chosen = maxItems - 1; canDraw = true; }
    if (down && !downL) { chosen++; if (chosen >= maxItems) chosen = 0; canDraw = true; }
  } else if (mode == 13) {
    // Keyboard navigation
    if (up && !upL) { kbdRow--; if (kbdRow < 0) kbdRow = 4; canDraw = true; }
    if (down && !downL) { kbdRow++; if (kbdRow > 4) kbdRow = 0; canDraw = true; }
    if (left && !leftL) { if (kbdRow < 4) { kbdCol--; if (kbdCol < 0) kbdCol = 9; } else { kbdCol -= 2; if (kbdCol < 0) kbdCol = 8; } canDraw = true; }
    if (right && !rightL) { if (kbdRow < 4) { kbdCol++; if (kbdCol > 9) kbdCol = 0; } else { kbdCol += 2; if (kbdCol > 9) kbdCol = 0; } canDraw = true; }
    if (a && !aL) { inputField = 1 - inputField; canDraw = true; }
  }

  // Selection Logic
  if (pressOk) {
    if (mode == 1) { // Main Menu
      if (chosen == 0) { // Scan
        tft.fillScreen(BLACK); tft.setCursor(10, 140); tft.print("Scanning...");
        scanCount = WiFi.scanNetworks();
        mode = 10; chosen = 0;
      } else if (chosen == 1) { mode = 11; chosen = 0; }
      else { mode = 13; chosen = 0; } // Manual
      canDraw = true;
    } else if (mode == 10) { // Scan Results
      ssid = WiFi.SSID(chosen);
      if (WiFi.encryptionType(chosen) == WIFI_AUTH_OPEN) {
        WiFi.begin(ssid.c_str());
        tft.fillScreen(BLACK); tft.setCursor(10, 140); tft.print("Connecting...");
        saveWiFiCredentials(); mode = 0;
      } else {
        mode = 13; inputField = 1; password = ""; // Go to keyboard for pass
      }
      canDraw = true;
    } else if (mode == 11) { // Saved Nets
      if (savedCount > 0) {
        ssid = savedNets[chosen].ssid;
        password = savedNets[chosen].pass;
        selectedNet = chosen;
        mode = 12; chosen = 0;
      } else { mode = 1; chosen = 0; }
      canDraw = true;
    } else if (mode == 12) { // Options
      if (chosen == 0) { // Connect
        WiFi.begin(ssid.c_str(), password.c_str());
        mode = 0;
      } else if (chosen == 1) { mode = 13; inputField = 1; }
      else if (chosen == 2) { forgetWiFi(selectedNet); mode = 11; chosen = 0; }
      else { mode = 11; chosen = 0; }
      canDraw = true;
    } else if (mode == 13) { // Keyboard
      if (kbdRow == 4) {
        int r4 = kbdCol / 2;
        if (r4 == 0) { isCaps = !isCaps; }
        else if (r4 == 1) { (inputField==0?ssid:password) += " "; }
        else if (r4 == 2) { String& s = (inputField==0?ssid:password); if(s.length()>0) s.remove(s.length()-1); }
        else if (r4 == 3) { mode = 1; forceRedraw = true; } // Update WiFi Menu
        else if (r4 == 4) { // DONE
          WiFi.begin(ssid.c_str(), password.c_str());
          saveWiFiCredentials(); mode = 0; forceRedraw = true; // Redraw radio
        }
      } else {
        char c = isCaps ? kbdChars[kbdRow][kbdCol] : kbdCharsLow[kbdRow][kbdCol];
        (inputField == 0 ? ssid : password) += c;
      }
      canDraw = true;
    }
  }

  upL=up; downL=down; leftL=left; rightL=right; aL=a; bL=b; selectL=select; menuL=menu; startL=start;
  
  if (canDraw) {
    if (mode == 1) drawWiFiMenu();
    else if (mode == 10) drawWiFiScan();
    else if (mode == 11) drawSavedNets();
    else if (mode == 12) drawNetOptions();
    else if (mode == 13) drawKeyboardUI();
    canDraw = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  initButtons();
  
  loadWiFiCredentials();

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setRotation(2);
  tft.fillScreen(BLACK);
  
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  Serial.println("TFT OK");
  
  tft.setTextSize(2);
  tft.setCursor(10, 140);
  tft.setTextColor(TFT_GREEN);
  tft.println("Starting...");
  delay(500);

  WiFi.mode(WIFI_STA);
  if (savedCount > 0) {
    for (int i = 0; i < savedCount; i++) {
      wifiMulti.addAP(savedNets[i].ssid, savedNets[i].pass);
    }
  } else {
    wifiMulti.addAP(ssid.c_str(), password.c_str());
  }
  
  tft.setCursor(10, 170);
  tft.println("WiFi...");
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
    delay(500);
    wifiAttempts++;
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected!");
    tft.setCursor(10, 200);
    tft.println("WiFi OK!");
  } else {
    tft.setCursor(10, 200);
    tft.setTextColor(TFT_RED);
    tft.println("WiFi FAIL!");
  }
  delay(500);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  audio.setVolume(volume);
  audio.connecttohost(stationUrls[0]);
  
  tft.setCursor(10, 230);
  tft.setTextColor(TFT_GREEN);
  tft.println("Playing!");
  delay(500);
  
  drawDone = false;
  canDraw = true;
  
  Serial.println("Setup complete");
}

void drawUI() {
  if (!canDraw && !forceRedraw) return;
  
  uint16_t gray = tft.color565(40, 40, 80);
  uint16_t light = tft.color565(100, 100, 140);
  tft.setTextSize(1);

  if (forceRedraw) {
    tft.fillRect(0, 0, 240, 320, gray);
    // Borders
    tft.fillRect(5, 25, 150, 200, BLACK);
    tft.drawRect(5, 25, 150, 200, light);
    tft.fillRect(160, 25, 75, 55, BLACK);
    tft.drawRect(160, 25, 75, 55, light);
    tft.fillRect(160, 90, 75, 20, BLACK);
    tft.drawRect(160, 90, 75, 20, light);
    tft.fillRect(5, 235, 230, 40, BLACK);
    tft.drawRect(5, 235, 230, 40, light);
    // Static Labels
    tft.setTextColor(light, gray);
    tft.setCursor(10, 5); tft.print("STATIONS");
    tft.setCursor(160, 130); tft.print("MENU:Settings");
    tft.setCursor(160, 145); tft.print("UP/DOWN:Station");
    tft.setCursor(160, 160); tft.print("L/R:Volume");
    tft.setCursor(160, 175); tft.print("A:Play/Pause");
    tft.setCursor(160, 190); tft.print("B:Sleep");
    forceRedraw = false;
  }

  // Update Stations
  if (chosen != lastChosen) {
    for (int i = 0; i < ns; i++) {
      if (i == chosen) {
        tft.fillRect(7, 28 + i * 30, 146, 28, TFT_GREEN);
        tft.setTextColor(BLACK, TFT_GREEN);
      } else {
        tft.fillRect(7, 28 + i * 30, 146, 28, BLACK);
        tft.setTextColor(TFT_WHITE, BLACK);
      }
      tft.setCursor(10, 35 + i * 30);
      tft.print(stations[i]);
    }
    lastChosen = chosen;
  }

  // Update Status
  if (rssi != lastRSSI || bitrate != lastBitrate) {
    tft.setTextColor(light, BLACK);
    tft.fillRect(165, 30, 65, 45, BLACK);
    tft.setCursor(165, 32); tft.print("RSSI:"); tft.println(rssi);
    tft.setCursor(165, 47); tft.print("BR:"); tft.println(bitrate);
    lastRSSI = rssi; lastBitrate = bitrate;
  }

  // Update Volume
  if (volume != lastVolume) {
    tft.setTextColor(TFT_WHITE, BLACK);
    tft.fillRect(195, 95, 35, 12, BLACK);
    tft.setCursor(165, 97); tft.print("VOL:"); tft.print(volume);
    lastVolume = volume;
  }

  // Update Now Playing
  if (songPlaying != lastSongPlaying) {
    tft.setTextColor(TFT_GREEN, BLACK);
    tft.fillRect(10, 245, 220, 20, BLACK);
    tft.setCursor(10, 245);
    tft.print("NOW:"); tft.print(songPlaying.substring(0, 25));
    lastSongPlaying = songPlaying;
  }

  canDraw = false;
}

void loop() {
  if (mode != 0) {
    handleWiFiInput();
    delay(10);
    return;
  }
  
  // Check buttons
  bool up = (digitalRead(KEY_UP) == LOW);
  bool down = (digitalRead(KEY_DOWN) == LOW);
  bool left = (digitalRead(KEY_LEFT) == LOW);
  bool right = (digitalRead(KEY_RIGHT) == LOW);
  bool a = (digitalRead(KEY_A) == LOW);
  bool b = (digitalRead(KEY_B) == LOW);
  bool menu = (digitalRead(KEY_MENU) == LOW);
  bool start = (digitalRead(KEY_START) == LOW);  // OK giua: play/pause

  if ((up||down||left||right||a||b||menu) && (millis() - lastBtnTime < DEBOUNCE_MS)) {
    // skip during debounce
  } else {
    if (menu && !btnPressed[6]) {
      btnPressed[6] = true; lastBtnTime = millis();
      mode = 1; chosen = 0;
      forceRedraw = true; // For WiFi menu transition
    } else if (!menu) btnPressed[6] = false;

    if (up && !btnPressed[0]) {
      btnPressed[0] = true; lastBtnTime = millis();
      chosen++; if (chosen >= ns) chosen = 0;
      audio.connecttohost(stationUrls[chosen]);
      canDraw = true;
    } else if (!up) btnPressed[0] = false;

    if (down && !btnPressed[1]) {
      btnPressed[1] = true; lastBtnTime = millis();
      chosen--; if (chosen < 0) chosen = ns - 1;
      audio.connecttohost(stationUrls[chosen]);
      canDraw = true;
    } else if (!down) btnPressed[1] = false;

    if (right && !btnPressed[2]) {
      btnPressed[2] = true; lastBtnTime = millis();
      volume++; if (volume > 21) volume = 21;
      audio.setVolume(volume);
      canDraw = true;
    } else if (!right) btnPressed[2] = false;

    if (left && !btnPressed[3]) {
      btnPressed[3] = true; lastBtnTime = millis();
      volume--; if (volume < 0) volume = 0;
      audio.setVolume(volume);
      canDraw = true;
    } else if (!left) btnPressed[3] = false;

    if (start && !btnPressed[4]) {
      btnPressed[4] = true; lastBtnTime = millis();
      audio.pauseResume();
      canDraw = true;
    } else if (!start) btnPressed[4] = false;

    if (a && !btnPressed[7]) {  // Back: ve man hinh radio chinh
      btnPressed[7] = true; lastBtnTime = millis();
      mode = 0; forceRedraw = true; canDraw = true;
    } else if (!a) btnPressed[7] = false;

    if (b && !btnPressed[5]) {  // Sleep (giu nut nguon vat ly)
      btnPressed[5] = true; lastBtnTime = millis();
      esp_deep_sleep_start();
    } else if (!b) btnPressed[5] = false;
  }

  // Update RSSI every 2 seconds
  static unsigned long lastRSSITime = 0;
  if (millis() - lastRSSITime > 2000) {
    lastRSSITime = millis();
    rssi = WiFi.RSSI();
    connected = (WiFi.status() == WL_CONNECTED);
    canDraw = true;
  }

  // Redraw UI if needed
  if (canDraw) {
    drawUI();
  }

  audio.loop();
  delay(10);
}

void audio_info(const char *info) {
  Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  songPlaying = info;
  canDraw = true;
}
void audio_bitrate(const char *info) {
  bitrate = (String(info).toInt() / 1000);
}
void audio_showstation(const char *info) {
  curStation = info;
}
