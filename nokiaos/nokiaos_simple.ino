/*
 * NokiaOS Simple - Test version
 * Chỉ test TFT và Buttons trước
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

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

#define NOKIA_BLUE 0x001F
#define NOKIA_WHITE 0xFFFF
#define NOKIA_BLACK 0x0000
#define NOKIA_GREEN 0x07E0
#define NOKIA_RED 0xF800

TFT_eSPI tft;

const char* menuItems[] = {
    "1. File Manager",
    "2. WiFi",
    "3. Bluetooth",
    "4. Media",
    "5. Browser",
    "6. GPIO",
    "7. Lua",
    "8. 3D Viewer",
    "9. Calculator",
    "10. Settings"
};
const int MENU_COUNT = 10;
int selectedIndex = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("NokiaOS Simple Test");
    
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
    
    Serial.println("Init TFT...");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(NOKIA_BLACK);
    
    Serial.println("Init LED...");
    pinMode(39, OUTPUT);
    digitalWrite(39, HIGH);
    
    Serial.println("Draw menu...");
    drawMenu();
    
    Serial.println("Ready!");
}

void loop() {
    if (digitalRead(KEY_UP) == LOW) {
        delay(50);
        if (digitalRead(KEY_UP) == LOW) {
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = MENU_COUNT - 1;
            drawMenu();
            while (digitalRead(KEY_UP) == LOW) delay(10);
        }
    }
    
    if (digitalRead(KEY_DOWN) == LOW) {
        delay(50);
        if (digitalRead(KEY_DOWN) == LOW) {
            selectedIndex++;
            if (selectedIndex >= MENU_COUNT) selectedIndex = 0;
            drawMenu();
            while (digitalRead(KEY_DOWN) == LOW) delay(10);
        }
    }
    
    // START giua = OK (Symbian map moi)
    if (digitalRead(KEY_START) == LOW) {
        delay(50);
        if (digitalRead(KEY_START) == LOW) {
            tft.fillScreen(NOKIA_BLUE);
            tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(menuItems[selectedIndex], tft.width()/2, tft.height()/2, 4);
            tft.setTextDatum(TL_DATUM);
            delay(2000);
            drawMenu();
            while (digitalRead(KEY_START) == LOW) delay(10);
        }
    }

    // A = Back, MENU = Home, B = Delete, SELECT = Game/T9 toggle
    if (digitalRead(KEY_A) == LOW || digitalRead(KEY_MENU) == LOW) {
        delay(50);
        if (digitalRead(KEY_A) == LOW || digitalRead(KEY_MENU) == LOW) {
            selectedIndex = 0;
            drawMenu();
            while (digitalRead(KEY_A) == LOW || digitalRead(KEY_MENU) == LOW) delay(10);
        }
    }

    delay(10);
}

void drawMenu() {
    tft.fillScreen(NOKIA_BLACK);
    
    tft.fillRect(0, 0, tft.width(), 28, NOKIA_BLUE);
    tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("NokiaOS v1.0", tft.width()/2, 8, 4);
    
    tft.setTextDatum(TL_DATUM);
    
    for (int i = 0; i < MENU_COUNT; i++) {
        int y = 35 + i * 24;
        
        if (i == selectedIndex) {
            tft.fillRect(0, y, tft.width(), 24, NOKIA_BLUE);
            tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
        } else {
            tft.fillRect(0, y, tft.width(), 24, NOKIA_BLACK);
            tft.setTextColor(NOKIA_WHITE, NOKIA_BLACK);
        }
        
        tft.setTextSize(2);
        tft.drawString(menuItems[i], 10, y + 4, 4);
    }
    
    tft.fillRect(0, tft.height() - 20, tft.width(), 20, NOKIA_BLUE);
    tft.setTextColor(NOKIA_WHITE, NOKIA_BLUE);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "Uptime: %lu s", millis()/1000);
    tft.drawString(timeStr, tft.width()/2, tft.height() - 12, 2);
    tft.setTextDatum(TL_DATUM);
}
