#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include "component/Display.h"
#include <vector>

#define BTN_UP    40
#define BTN_DOWN  5
#define BTN_A     37
#define BTN_B     36
#define SD_CS     10
#define SD_MOSI   11
#define SD_SCLK   13
#define SD_MISO   9
#define BUZZER    41

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
SPIClass sdSPI = SPIClass(HSPI);

struct ButtonState {
    bool current;
    bool just_pressed;
    bool last;
};

ButtonState btnUp = {false, false, false};
ButtonState btnDown = {false, false, false};
ButtonState btnA = {false, false, false};
ButtonState btnB = {false, false, false};

String currentPath = "/";
std::vector<String> fileList;
int selectedIdx = 0;
int scrollOffset = 0;
const int itemsPerPage = 5;
const int itemHeight = 44;

bool running = false;
float elapsed = 0.0;
float lapStart = 0.0;
int lapCount = 0;
const int MAX_LAPS = 20;
float lapTimes[MAX_LAPS];
float lapTotals[MAX_LAPS];

void updateButtons() {
    bool up = digitalRead(BTN_UP) == LOW;
    bool down = digitalRead(BTN_DOWN) == LOW;
    bool a = digitalRead(BTN_A) == LOW;
    bool b = digitalRead(BTN_B) == LOW;

    btnUp.just_pressed = up && !btnUp.last;
    btnUp.last = up;
    btnUp.current = up;

    btnDown.just_pressed = down && !btnDown.last;
    btnDown.last = down;
    btnDown.current = down;

    btnA.just_pressed = a && !btnA.last;
    btnA.last = a;
    btnA.current = a;

    btnB.just_pressed = b && !btnB.last;
    btnB.last = b;
    btnB.current = b;
}

void beep(int ms = 30) {
    digitalWrite(BUZZER, HIGH);
    delay(ms);
    digitalWrite(BUZZER, LOW);
}

String formatTime(float t) {
    int minutes = (int)(t / 60);
    int seconds = (int)((int)t % 60);
    int ms = (int)(t * 100) % 100;
    char buf[10];
    sprintf(buf, "%02d:%02d.%02d", minutes, seconds, ms);
    return String(buf);
}

void drawStopwatch() {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(50, 30);
    tft.print(formatTime(elapsed));
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(50, 80);
    tft.print("LAP    LAP TIME    TOTAL");
    
    int y = 100;
    int visibleLaps = min(lapCount, 6);
    int startLap = max(0, lapCount - visibleLaps);
    
    tft.setTextColor(TFT_YELLOW);
    for (int i = 0; i < visibleLaps; i++) {
        int idx = startLap + i;
        tft.setCursor(50, y);
        tft.print("#" + String(idx + 1));
        tft.setCursor(95, y);
        tft.print(formatTime(lapTimes[idx]));
        tft.setCursor(175, y);
        tft.print(formatTime(lapTotals[idx]));
        y += 18;
    }
    
    int btnY = 210;
    uint16_t startColor = running ? TFT_GREEN : TFT_DARKGREY;
    
    tft.fillCircle(60, btnY, 12, startColor);
    tft.setTextColor(running ? TFT_BLACK : TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(80, btnY - 4);
    tft.print(running ? "STOP" : "START");
    
    tft.fillCircle(60, btnY + 35, 12, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(80, btnY + 31);
    tft.print("LAP");
    
    tft.fillCircle(180, btnY, 12, TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(200, btnY - 4);
    tft.print("EXIT");
}

void runStopwatch() {
    elapsed = 0.0;
    lapStart = 0.0;
    lapCount = 0;
    running = false;
    
    unsigned long lastTime = millis();
    
    while (true) {
        updateButtons();
        
        if (btnB.just_pressed) {
            beep(30);
            break;
        }
        
        if (btnA.just_pressed) {
            running = !running;
            beep(20);
            lastTime = millis();
        }
        
        if (running) {
            unsigned long now = millis();
            float delta = (now - lastTime) / 1000.0f;
            lastTime = now;
            elapsed += delta;
        }
        
        if (btnDown.just_pressed && running && lapCount < MAX_LAPS) {
            lapTimes[lapCount] = elapsed - lapStart;
            lapTotals[lapCount] = elapsed;
            lapStart = elapsed;
            lapCount++;
            beep(10);
        }
        
        if (btnUp.just_pressed && !running && lapCount > 0) {
            elapsed = 0.0;
            lapStart = 0.0;
            lapCount = 0;
            beep(10);
        }
        
        drawStopwatch();
        delay(16);
    }
}

void scanFiles() {
    fileList.clear();
    File root = SD.open(currentPath);
    if (!root || !root.isDirectory()) return;
    if (currentPath != "/") fileList.push_back("[DIR] ..");

    File entry = root.openNextFile();
    while (entry) {
        String name = String(entry.name());
        if (entry.isDirectory()) {
            fileList.push_back("[DIR] " + name);
        } else {
            String tag = "";
            if (name.endsWith(".lua")) tag = "[LUA] ";
            else if (name.endsWith(".txt")) tag = "[TXT] ";
            else if (name.endsWith(".js")) tag = "[JS] ";
            else tag = "[FILE] ";
            fileList.push_back(tag + name);
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
}

void drawUI() {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextSize(2);
    
    for (int i = 0; i < itemsPerPage; i++) {
        int idx = scrollOffset + i;
        if (idx >= (int)fileList.size()) break;
        
        int y = i * (itemHeight + 4) + 5;
        uint16_t color = (idx == selectedIdx) ? TFT_ORANGE : uint16_t(0x3186);
        
        canvas.drawRoundRect(5, y, 230, itemHeight, 6, color);
        if (idx == selectedIdx) {
            canvas.drawRoundRect(4, y - 1, 232, itemHeight + 2, 6, color);
        }
        
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(12, y + 14);
        
        String displayName = fileList[idx];
        if (displayName.length() > 18) displayName = displayName.substring(0, 15) + "...";
        canvas.print(displayName);
    }
    
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextSize(1);
    canvas.setCursor(5, 235);
    canvas.print("A:SELECT  B:STOPWATCH");
    
    if (fileList.size() > itemsPerPage) {
        int barH = 240 / fileList.size();
        canvas.fillRect(236, selectedIdx * (240 / fileList.size()), 3, barH, TFT_GOLD);
    }

    canvas.pushSprite(0, 0);
}

void setup() {
    Serial.begin(115200);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    canvas.createSprite(240, 240);

    sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSPI)) {
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.setCursor(0, 0);
        tft.println("SD CARD ERROR!");
        while (true) delay(1000);
    }

    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.println("SD OK!");
    tft.println("A: Run Lua");
    tft.println("B: Stopwatch");
    delay(500);

    scanFiles();
    drawUI();
}

void loop() {
    updateButtons();

    if (btnB.just_pressed) {
        beep(50);
        runStopwatch();
        drawUI();
    }

    if (btnUp.current && selectedIdx > 0) {
        selectedIdx--;
        if (selectedIdx < scrollOffset) scrollOffset--;
        beep(20);
        drawUI();
        delay(150);
    }
    if (btnDown.current && selectedIdx < (int)fileList.size() - 1) {
        selectedIdx++;
        if (selectedIdx >= scrollOffset + itemsPerPage) scrollOffset++;
        beep(20);
        drawUI();
        delay(150);
    }
    if (btnA.just_pressed) {
        String item = fileList[selectedIdx];
        if (item.startsWith("[DIR] ")) {
            String folder = item.substring(6);
            if (folder == "..") {
                int last = currentPath.lastIndexOf('/');
                currentPath = (last <= 0) ? "/" : currentPath.substring(0, last);
            } else {
                currentPath = (currentPath == "/") ? "/" + folder : currentPath + "/" + folder;
            }
            selectedIdx = 0;
            scrollOffset = 0;
            scanFiles();
            drawUI();
        } else {
            int firstSpace = item.indexOf(' ');
            String nameOnly = item.substring(firstSpace + 1);
            String fullPath = (currentPath == "/") ? "/" + nameOnly : currentPath + "/" + nameOnly;
            
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_YELLOW);
            tft.setCursor(0, 0);
            tft.print("File: ");
            tft.println(nameOnly);
            tft.setTextColor(TFT_WHITE);
            tft.println("");
            tft.println("Lua interpreter");
            tft.println("not installed.");
            tft.println("");
            tft.println("Press B for");
            tft.println("Stopwatch demo");
            
            while (!btnB.just_pressed && !btnA.just_pressed) {
                updateButtons();
                delay(50);
            }
            if (btnB.just_pressed) {
                runStopwatch();
            }
            drawUI();
        }
        delay(200);
    }
}
