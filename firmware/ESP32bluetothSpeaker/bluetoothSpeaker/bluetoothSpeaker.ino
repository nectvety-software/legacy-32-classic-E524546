/*
  ESP32-S3 + ST7789 2" TFT + 10 Buttons
  Rotation 0: 240x320
*/

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define TFT_BL 39

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

#define DEBOUNCE_DELAY 50

struct Button {
  uint8_t pin;
  bool lastState;
  unsigned long lastDebounceTime;
};

Button buttons[10];

enum Screen { SCREEN_SPLASH, SCREEN_MAIN, SCREEN_MENU, SCREEN_INFO };

Screen currentScreen = SCREEN_SPLASH;
int selectedMenuItem = 0;
int volume = 50;
bool isPlaying = false;
unsigned long splashStartTime = 0;

const char* menuItems[] = {"Volume+", "Volume-", "Info", "Bright+", "Bright-", "Back"};

void initButtons() {
  uint8_t pins[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_MENU, KEY_OPTION, KEY_SELECT, KEY_START, KEY_A, KEY_B};
  for (int i = 0; i < 10; i++) {
    pinMode(pins[i], INPUT_PULLUP);
    buttons[i].pin = pins[i];
    buttons[i].lastState = HIGH;
    buttons[i].lastDebounceTime = 0;
  }
}

bool isButtonPressed(int index) {
  bool reading = digitalRead(buttons[index].pin);
  unsigned long now = millis();
  
  if (reading != buttons[index].lastState) {
    buttons[index].lastDebounceTime = now;
  }
  
  if ((now - buttons[index].lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && buttons[index].lastState == HIGH) {
      buttons[index].lastState = LOW;
      return true;
    }
  }
  
  if (reading == HIGH) buttons[index].lastState = HIGH;
  return false;
}

void drawSplashScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawCentreString("BT", 120, 80, 1);
  tft.drawCentreString("SPEAKER", 120, 115, 1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawCentreString("ESP32-S3", 120, 160, 1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawCentreString("ST7789 2.0 inch", 120, 200, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Waiting...", 120, 240, 1);
}

void drawMainScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 240, 35, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawCentreString("BT Speaker", 120, 5, 1);
  
  tft.drawLine(0, 35, 240, 35, TFT_WHITE);
  
  if (isPlaying) {
    tft.fillCircle(120, 85, 35, TFT_BLUE);
    tft.fillCircle(120, 85, 25, TFT_CYAN);
    tft.fillTriangle(112, 72, 112, 98, 135, 85, TFT_BLACK);
  } else {
    tft.fillCircle(120, 85, 35, TFT_DARKGREY);
    tft.fillCircle(120, 85, 25, TFT_DARKGREY);
    tft.fillRect(105, 72, 10, 26, TFT_BLACK);
    tft.fillRect(125, 72, 10, 26, TFT_BLACK);
  }
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawCentreString("STATUS: Standby", 120, 135, 1);
  tft.drawCentreString("Connect via BT", 120, 150, 1);
  
  tft.drawLine(0, 170, 240, 170, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Vol:", 10, 185, 1);
  tft.fillRect(45, 183, 130, 10, TFT_DARKGREY);
  tft.fillRect(45, 183, volume * 130 / 100, 10, TFT_GREEN);
  tft.drawNumber(volume, 180, 185, 1);
  tft.drawString("%", 200, 185, 1);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawCentreString("[MENU] Settings", 120, 210, 1);
  tft.drawCentreString("[A] Toggle  [<][>] Vol", 120, 225, 1);
}

void drawMenuScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 240, 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawCentreString("MENU", 120, 3, 1);
  
  for (int i = 0; i < 6; i++) {
    int yPos = 40 + i * 45;
    tft.fillRect(5, yPos, 230, 35, (i == selectedMenuItem) ? TFT_BLUE : TFT_BLACK);
    tft.setTextColor((i == selectedMenuItem) ? TFT_BLACK : TFT_WHITE, (i == selectedMenuItem) ? TFT_BLUE : TFT_BLACK);
    tft.setTextSize(2);
    tft.drawCentreString(menuItems[i], 120, yPos + 6, 1);
  }
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawCentreString("[A] Back  [START] OK", 120, 295, 1);
}

void drawInfoScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 240, 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawCentreString("INFO", 120, 3, 1);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Device:", 10, 45, 1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("ESP32-BT-Speaker", 10, 58, 1);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Display:", 10, 85, 1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("ST7789 2.0 inch", 10, 98, 1);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Resolution:", 10, 125, 1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("240 x 320 pixels", 10, 138, 1);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Buttons:", 10, 165, 1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("10 controls", 10, 178, 1);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Board:", 10, 205, 1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("ESP32-S3 N16R8", 10, 218, 1);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString("[A] Back  [START] OK", 120, 295, 1);
}

void handleButtonInput() {
  if (isButtonPressed(4)) {
    if (currentScreen == SCREEN_MAIN) {
      currentScreen = SCREEN_MENU;
      selectedMenuItem = 0;
      drawMenuScreen();
    } else if (currentScreen != SCREEN_SPLASH) {
      currentScreen = SCREEN_MAIN;
      drawMainScreen();
    }
  }
  // A (Back) ve MAIN tu moi man hinh (Symbian map moi).
  if (isButtonPressed(8) && currentScreen != SCREEN_MAIN && currentScreen != SCREEN_SPLASH) {
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
  }
  
  if (currentScreen == SCREEN_MENU) {
    if (isButtonPressed(0)) { selectedMenuItem = (selectedMenuItem + 5) % 6; drawMenuScreen(); }
    if (isButtonPressed(1)) { selectedMenuItem = (selectedMenuItem + 1) % 6; drawMenuScreen(); }
    if (isButtonPressed(7)) {
      switch (selectedMenuItem) {
        case 0: volume = min(100, volume + 10); drawMenuScreen(); return;
        case 1: volume = max(0, volume - 10); drawMenuScreen(); return;
        case 2: currentScreen = SCREEN_INFO; drawInfoScreen(); return;
        case 3: digitalWrite(TFT_BL, HIGH); return;
        case 4: digitalWrite(TFT_BL, LOW); return;
        case 5: break;
      }
      currentScreen = SCREEN_MAIN; drawMainScreen();
    }
  }
  
  if (currentScreen == SCREEN_MAIN) {
    if (isButtonPressed(2)) { volume = max(0, volume - 5); drawMainScreen(); }
    if (isButtonPressed(3)) { volume = min(100, volume + 5); drawMainScreen(); }
    if (isButtonPressed(8)) { isPlaying = !isPlaying; drawMainScreen(); }
  }
}

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  initButtons();
  drawSplashScreen();
  splashStartTime = millis();
}

void loop() {
  if (currentScreen == SCREEN_SPLASH && millis() - splashStartTime > 2000) {
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
  }
  handleButtonInput();
  delay(10);
}
