/*
 * LegacyOs v4.1 - Keira-inspired Architecture for ESP32-S3 CyberBox
 * Hardware: ESP32-S3-WROOM-1 (N16R8), ST7789 (240x240), BME280, microSD, Buzzer, 6-btn
 * Framework: Arduino IDE + TFT_eSPI + HSPI @ 80MHz
 *
 * App lifecycle: create -> update + draw -> destroy
 * Architecture: AppManager + ServiceManager + StatusBar overlay + Alert modal
 */
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <SD.h>
#include "generated_icons.h"
#include "keira.h"

// ===================== PIN DEFINITIONS =====================
#define TFT_BL    39
#define SD_CS     10
#define SD_MOSI   11
#define SD_SCLK   13
#define SD_MISO   9
#define SD_DET    38
#define I2C_SDA   18
#define I2C_SCL   6
#define BUZZ_PIN  41
#define PIN_BAT   1
#define PIN_PGOOD 21
#define PIN_CHG   42

// ===================== DISPLAY GLOBALS =====================
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite cv = TFT_eSprite(&tft);

void dispPush() { cv.pushSprite(0, 0); }

// ===================== SOUND =====================
void snd(int f, int d) { tone(BUZZ_PIN, f, d); }
void sndNav()  { snd(1600, 15); }
void sndOk()   { snd(2400, 50); delay(50); snd(3200, 50); }
void sndBack() { snd(600, 75); delay(75); snd(450, 75); }
void sndBoot() { snd(800, 100); delay(100); snd(1200, 100); delay(100); snd(1800, 200); }

// ===================== CONTROLLER =====================
Controller ctl;

void Controller::begin() {
    for (int i = 0; i < BTN_COUNT; i++) {
        pinMode(pins[i], INPUT_PULLUP);
        btns[i] = {0, false, false, false};
        lastDebounce[i] = 0; nextRepeat[i] = 0; repeatActive[i] = false;
    }
}

void Controller::update() {
    uint32_t now = millis();
    for (int i = 0; i < BTN_COUNT; i++) {
        bool raw = (digitalRead(pins[i]) == LOW);
        btns[i].justPressed = false;
        btns[i].justReleased = false;
        if (raw != btns[i].pressed) {
            if (now - lastDebounce[i] > DEBOUNCE) {
                lastDebounce[i] = now;
                if (raw) {
                    btns[i].pressed = true; btns[i].justPressed = true;
                    btns[i].lastChange = now; nextRepeat[i] = now + REPEAT_DELAY;
                    repeatActive[i] = false;
                } else {
                    btns[i].pressed = false; btns[i].justReleased = true;
                    btns[i].lastChange = now; nextRepeat[i] = 0;
                    repeatActive[i] = false;
                }
            }
        } else if (raw && btns[i].pressed && now >= nextRepeat[i] && nextRepeat[i] > 0) {
            btns[i].justPressed = true;
            nextRepeat[i] = now + (repeatActive[i] ? REPEAT_RATE : REPEAT_RATE);
            repeatActive[i] = true;
        }
    }
}

// ===================== ICON SYSTEM =====================
const int APP_COUNT = 8;
AppEntry apps[APP_COUNT] = {
    {"Devices",  "GPIO",   0},
    {"Wi-Fi",    "Net",    1},
    {"Files",    "SD",     2},
    {"Weather",  "BME280", 3},
    {"Tools",    "Utils",  4},
    {"Settings", "Config", 5},
    {"Sys Info", "ESP32",  6},
    {"About",    "v4.1",   7},
};
TFT_eSprite* iconCache[APP_COUNT];

void initIcons() {
    for (int i = 0; i < APP_COUNT; i++) {
        iconCache[i] = new TFT_eSprite(&tft);
        iconCache[i]->createSprite(24, 24);
        memcpy_P(iconCache[i]->getPointer(), icon_data[i], 24 * 24 * sizeof(uint16_t));
    }
}

// ===================== MENU =====================
Menu::Menu() {
    _title = "Menu"; _count = 0; _cursor = 0; _scroll = 0;
    _finished = false; _activationButton = (int)Controller::OK;
    _titleColor = C_WHITE; _lastMove = 0;
}

void Menu::clear() { _count = 0; _cursor = 0; _scroll = 0; _finished = false; }

bool Menu::isFinished() { bool f = _finished; if (f) _finished = false; return f; }

void Menu::reset() { _finished = false; _cursor = 0; _scroll = 0; }

void Menu::addItem(const char* title, uint16_t color, const char* postfix,
                   int iconIdx, void (*cb)(int), int cd) {
    if (_count >= MENU_MAX_ITEMS) return;
    _items[_count] = {title, color, postfix, iconIdx, cb, cd}; _count++;
}

void Menu::update() {
    if (ctl.pressed(Controller::UP)) { _cursor = (_cursor > 0) ? _cursor - 1 : _count - 1; _lastMove = millis(); sndNav(); }
    if (ctl.pressed(Controller::DOWN)) { _cursor = (_cursor < _count - 1) ? _cursor + 1 : 0; _lastMove = millis(); sndNav(); }
    if (ctl.pressed(Controller::LEFT)) { _cursor = (_cursor - VISIBLE < 0) ? 0 : _cursor - VISIBLE; _lastMove = millis(); sndNav(); }
    if (ctl.pressed(Controller::RIGHT)) { _cursor = (_cursor + VISIBLE >= _count) ? _count - 1 : _cursor + VISIBLE; _lastMove = millis(); sndNav(); }
    if (ctl.pressed((Controller::Button)_activationButton)) {
        _finished = true; sndOk();
        if (_items[_cursor].callback) _items[_cursor].callback(_items[_cursor].callbackData);
    }
    if (_cursor < _scroll) _scroll = _cursor;
    if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
    if (_scroll > _count - VISIBLE) _scroll = (_count > VISIBLE) ? _count - VISIBLE : 0;
}

void Menu::draw(TFT_eSprite* gfx) {
    gfx->fillSprite(C_BLACK);
    int angleShift = (int)(sin(millis() / 500.0f) * 8);
    gfx->fillTriangle(0, 0, 48 - angleShift, 0, 0, 48 + angleShift, C_BLUE);
    gfx->fillTriangle(W, 0, W - 48 + angleShift, 0, W, 48 - angleShift, C_YELLOW);
    gfx->setFreeFont(&FreeSansBold12pt7b);
    gfx->setTextColor(_titleColor, C_BLACK);
    gfx->setCursor(16, 28);
    gfx->print(_title);
    int sbTop = TITLE_H, sbH = H - TITLE_H;
    int sbBarH = (_count > 0) ? max(8, (sbH * VISIBLE) / max(1, _count)) : sbH;
    int sbBarY = sbTop + ((_count > 1) ? (_cursor * (sbH - sbBarH)) / max(1, _count - 1) : 0);
    gfx->drawFastVLine(W - 5, sbTop, sbH, C_DIMGRAY);
    gfx->fillRect(W - 8, sbBarY, 8, sbBarH, _titleColor);
    for (int i = 0; i < VISIBLE && (i + _scroll) < _count; i++) {
        int idx = i + _scroll, iy = ITEMS_Y + i * ITEM_H;
        bool sel = (idx == _cursor);
        if (sel) gfx->fillRect(0, iy, W - SCROLL_W, ITEM_H, C_ORANGERED);
        if (_items[idx].iconIdx >= 0 && _items[idx].iconIdx < APP_COUNT)
            gfx->pushImage(4, iy, 24, 24, (uint16_t*)iconCache[_items[idx].iconIdx]->getPointer(), (uint16_t)C_BLACK);
        if (_items[idx].postfix && strlen(_items[idx].postfix) > 0) {
            gfx->setFreeFont(&FreeMono9pt7b);
            gfx->setTextColor(sel ? C_BLACK : C_CYAN, C_BLACK);
            int pw = strlen(_items[idx].postfix) * 7;
            gfx->setCursor(W - SCROLL_W - pw - 8, iy + 18);
            gfx->print(_items[idx].postfix);
        }
        gfx->setFreeFont(&FreeSans9pt7b);
        gfx->setTextColor(sel ? C_BLACK : _items[idx].color, C_BLACK);
        gfx->setCursor(ICON_W + 4, iy + 18);
        gfx->print(_items[idx].title);
    }
}

// ===================== STATUS BAR =====================
char timeStr[6] = "--:--";
uint32_t lastTimeUpdate = 0;

void drawStatusBar(TFT_eSprite* gfx) {
    gfx->fillRect(0, 0, W, HEADER_H, C_BLACK);
    gfx->setFreeFont(&FreeSansBold12pt7b);
    gfx->setTextColor(C_CYAN, C_BLACK);
    gfx->setCursor(60, 17);
    gfx->print("LEGACY OS");
    uint32_t now = millis();
    if (now - lastTimeUpdate > 10000) {
        lastTimeUpdate = now;
        int hr = ((now / 3600000) + 8) % 24;
        int mn = (now / 60000) % 60;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hr, mn);
    }
    gfx->setFreeFont(&FreeSans9pt7b);
    gfx->setTextColor(C_DARKORANGE, C_BLACK);
    gfx->setCursor(4, 36);
    gfx->print("--C");
    gfx->setTextColor(C_WHITE, C_BLACK);
    gfx->setCursor(W / 2 - 18, 36);
    gfx->print(timeStr);
    int raw = 0;
    for (int i = 0; i < 16; i++) raw += analogRead(PIN_BAT);
    raw /= 16;
    float voltage = (raw / 4095.0f) * 3.3f * 2.0f;
    int bat = (int)((voltage - 3.0f) / (4.2f - 3.0f) * 100);
    if (bat > 100) bat = 100;
    if (bat < 0) bat = 0;
    char batStr[4];
    snprintf(batStr, sizeof(batStr), "%d%%", bat);
    gfx->setTextColor(C_GREEN, C_BLACK);
    gfx->setCursor(W - 34, 36);
    gfx->print(batStr);
    gfx->drawFastHLine(0, HEADER_H, W, C_GRAY44);
}

// ===================== ALERT =====================
Alert alert;

void Alert::update() {
    if (_active && (ctl.pressed(Controller::OK) || ctl.pressed(Controller::BACK))) {
        _active = false; sndOk();
    }
}

void Alert::draw(TFT_eSprite* gfx) {
    if (!_active) return;
    int bw = W - 20, bh = 80, bx = 10, by = (H - bh) / 2;
    gfx->fillRoundRect(bx, by, bw, bh, 6, C_MIDNIGHTBL);
    gfx->drawRoundRect(bx, by, bw, bh, 6, C_BORDERGRAY);
    gfx->setFreeFont(&FreeSans9pt7b);
    gfx->setTextColor(C_ORANGERED, C_MIDNIGHTBL);
    gfx->setCursor(bx + 10, by + 20);
    gfx->print(_title);
    gfx->drawFastHLine(bx + 8, by + 26, bw - 16, C_BORDERGRAY);
    gfx->setTextColor(C_WHITE, C_MIDNIGHTBL);
    gfx->setCursor(bx + 10, by + 48);
    gfx->print(_body);
    gfx->setCursor(bx + 10, by + 66);
    gfx->setTextColor(C_CYAN, C_MIDNIGHTBL);
    gfx->print("[ OK ]");
}

// ===================== LAUNCHER APP =====================
LauncherApp launcherApp;
static void launchAppCallback(int idx);

void LauncherApp::create() { _selected = 0; }

void LauncherApp::update() {
    int row = _selected / 3, col = _selected % 3;
    if (ctl.pressed(Controller::UP))    { row = (row > 0) ? row - 1 : 2; _selected = row * 3 + col; sndNav(); }
    if (ctl.pressed(Controller::DOWN))  { row = (row < 2) ? row + 1 : 0; _selected = row * 3 + col; sndNav(); }
    if (ctl.pressed(Controller::LEFT))  { col = (col > 0) ? col - 1 : 2; _selected = row * 3 + col; sndNav(); }
    if (ctl.pressed(Controller::RIGHT)) { col = (col < 2) ? col + 1 : 0; _selected = row * 3 + col; sndNav(); }
    if (ctl.pressed(Controller::OK) && _selected < APP_COUNT) { sndOk(); launchAppCallback(_selected); }
}

void LauncherApp::draw(TFT_eSprite* gfx) {
    gfx->fillSprite(C_BLACK);
    const int MG = 5, GP = 4, COLS = 3, ROWS = 3;
    const int CW = (W - 2*MG - 2*GP) / COLS;
    const int CH = (H - HEADER_H - FOOTER_H - 2*MG - 2*GP) / ROWS;
    for (int i = 0; i < 9; i++) {
        int r = i / COLS, c = i % COLS;
        int cx = MG + c * (CW + GP), cy = HEADER_H + MG + r * (CH + GP);
        bool sel = (i == _selected);
        gfx->fillRoundRect(cx, cy, CW, CH, 4, C_GRAY33);
        if (sel) {
            gfx->drawRoundRect(cx - 1, cy - 1, CW + 2, CH + 2, 5, C_ORANGERED);
            gfx->drawRoundRect(cx - 3, cy - 3, CW + 6, CH + 6, 7, C_GLOW_ORANGE);
        }
        if (i < APP_COUNT && apps[i].iconIdx >= 0) {
            int ix = cx + (CW - 24) / 2;
            gfx->pushImage(ix, cy + 5, 24, 24, (uint16_t*)iconCache[apps[i].iconIdx]->getPointer(), (uint16_t)C_BLACK);
        }
        const char* lbl = (i < APP_COUNT) ? apps[i].name : "---";
        gfx->setFreeFont(&FreeSans9pt7b);
        gfx->setTextColor(sel ? C_ORANGERED : C_DIMGRAY, C_BLACK);
        gfx->setCursor(cx + (CW - strlen(lbl) * 6) / 2, cy + CH - 6);
        gfx->print(lbl);
    }
    // Footer telemetry bar
    int fy = H - FOOTER_H;
    gfx->fillRect(0, fy, W, FOOTER_H, C_BLACK);
    gfx->drawFastHLine(0, fy, W, C_GRAY44);
    gfx->setFreeFont(&FreeSans9pt7b);
    gfx->setTextColor(C_GREEN, C_BLACK);
    gfx->setCursor(6, fy + 15);
    gfx->print("T:--C | H:--% | P:--hPa");
}

// ===================== APP MANAGER =====================
AppManager appMgr(&launcherApp);

AppManager::AppManager(App* launcherApp) : _launcher(launcherApp), _current(nullptr), _pending(launcherApp), _redrawNeeded(true) {}

void AppManager::update() {
    if (_pending && _pending != _current) {
        if (_current) _current->destroy();
        _current = _pending;
        _pending = nullptr;
        _current->create();
        _redrawNeeded = true;
    }
}

void AppManager::back() {
    if (_current && _current != _launcher) {
        if (_current->onBack()) switchTo(_launcher);
    }
}

void AppManager::draw(TFT_eSprite* gfx) {
    if (_current) _current->draw(gfx);
}

// App instances for each grid cell
PlaceholderApp wifiApp("Wi-Fi Scanner");
PlaceholderApp filesApp("File Manager");
PlaceholderApp weatherApp("Weather Station");
PlaceholderApp toolsApp("Quick Tools");
PlaceholderApp settingsApp("Settings");
GPIOApp gpioApp;
SysInfoApp sysInfoApp;
AboutApp aboutApp;

void launchAppCallback(int idx) {
    App* targets[] = {&gpioApp, &wifiApp, &filesApp, &weatherApp, &toolsApp, &settingsApp, &sysInfoApp, &aboutApp};
    if (idx >= 0 && idx < APP_COUNT) appMgr.switchTo(targets[idx]);
}

// ===================== GPIO APP =====================
GPIOApp::GPIOApp() : _scroll(0), _cursor(0) {
    _pins[0] = BTN_OK;   _labels[0] = "KEY2 (OK)";
    _pins[1] = BTN_BACK; _labels[1] = "KEY1 (BACK)";
    _pins[2] = BUZZ_PIN; _labels[2] = "Buzzer";
    _pins[3] = PIN_BAT;  _labels[3] = "Batt ADC";
    _pins[4] = PIN_PGOOD;_labels[4] = "USB PWR";
    _pins[5] = PIN_CHG;  _labels[5] = "Charging";
    _pins[6] = SD_DET;   _labels[6] = "SD Det";
    _pins[7] = TFT_BL;   _labels[7] = "Backlight";
}

void GPIOApp::create() { _cursor = 0; _scroll = 0; }

void GPIOApp::update() {
    if (ctl.pressed(Controller::UP)) { _cursor--; if (_cursor < 0) _cursor = PINS - 1; sndNav(); }
    if (ctl.pressed(Controller::DOWN)) { _cursor = (_cursor + 1) % PINS; sndNav(); }
    if (_cursor < _scroll) _scroll = _cursor;
    if (_cursor >= _scroll + 6) _scroll = _cursor - 5;
}

void GPIOApp::draw(TFT_eSprite* gfx) {
    gfx->fillSprite(C_BLACK);
    gfx->setFreeFont(&FreeSansBold12pt7b);
    gfx->setTextColor(C_DARKORANGE, C_BLACK);
    gfx->setCursor(4, 50);
    gfx->print("GPIO Debug");
    gfx->drawFastHLine(0, 58, W, C_GRAY44);
    int vy = 68;
    for (int i = 0; i < 6 && (i + _scroll) < PINS; i++) {
        int idx = i + _scroll;
        bool sel = (idx == _cursor);
        if (sel) gfx->fillRect(0, vy - 2, W, 24, C_ORANGERED);
        int val = digitalRead(_pins[idx]);
        uint16_t col = val ? C_GREEN : C_RED;
        gfx->fillCircle(14, vy + 8, 6, col);
        gfx->setFreeFont(&FreeSans9pt7b);
        gfx->setTextColor(sel ? C_BLACK : C_WHITE, C_BLACK);
        gfx->setCursor(28, vy + 13);
        gfx->print(_labels[idx]);
        bool isAnalog = (_pins[idx] == PIN_BAT);
        if (isAnalog) {
            int araw = analogRead(PIN_BAT);
            gfx->setFreeFont(&FreeMono9pt7b);
            gfx->setTextColor(sel ? C_BLACK : C_CYAN, C_BLACK);
            gfx->setCursor(W - 60, vy + 13);
            gfx->print(araw);
        } else {
            gfx->setFreeFont(&FreeMono9pt7b);
            gfx->setTextColor(sel ? C_BLACK : C_CYAN, C_BLACK);
            gfx->setCursor(W - 40, vy + 13);
            gfx->print(val ? "HIGH" : "LOW ");
        }
        vy += 26;
    }
    gfx->setFreeFont(&FreeSans9pt7b);
    gfx->setTextColor(C_DIMGRAY, C_BLACK);
    gfx->setCursor(4, H - 8);
    gfx->print("BACK: Exit");
}

// ===================== SYS INFO APP =====================
void SysInfoApp::create() {}

void SysInfoApp::update() {}

void SysInfoApp::draw(TFT_eSprite* gfx) {
    gfx->fillSprite(C_BLACK);
    gfx->setFreeFont(&FreeSansBold12pt7b);
    gfx->setTextColor(C_CYAN, C_BLACK);
    gfx->setCursor(4, 50);
    gfx->print("System Info");
    gfx->drawFastHLine(0, 58, W, C_GRAY44);
    int y = 74;
    gfx->setFreeFont(&FreeSans9pt7b);
    auto line = [&](const char* label, const char* val) {
        gfx->setTextColor(C_BORDERGRAY, C_BLACK); gfx->setCursor(4, y); gfx->print(label);
        gfx->setTextColor(C_WHITE, C_BLACK); gfx->setCursor(W / 2, y); gfx->print(val);
        y += 20;
    };
    line("Chip:", "ESP32-S3");
    char buf[32];
    snprintf(buf, sizeof(buf), "%d MB", ESP.getFlashChipSize() / (1024 * 1024));
    line("Flash:", buf);
    snprintf(buf, sizeof(buf), "%d KB", ESP.getFreeHeap() / 1024);
    line("Free Heap:", buf);
    snprintf(buf, sizeof(buf), "%d KB", ESP.getHeapSize() / 1024);
    line("Total Heap:", buf);
    snprintf(buf, sizeof(buf), "%d KB", ESP.getPsramSize() / 1024);
    line("PSRAM:", buf);
    snprintf(buf, sizeof(buf), "%d MHz", ESP.getCpuFreqMHz());
    line("CPU:", buf);
    snprintf(buf, sizeof(buf), "%d KB", ESP.getSketchSize() / 1024);
    line("Sketch:", buf);
}

// ===================== ABOUT APP =====================
void AboutApp::create() {}
void AboutApp::update() {}

void AboutApp::draw(TFT_eSprite* gfx) {
    gfx->fillSprite(C_BLACK);
    gfx->setFreeFont(&FreeSansBold12pt7b);
    gfx->setTextColor(C_GREEN, C_BLACK);
    gfx->setCursor(30, 60);
    gfx->print("LEGACY OS");
    gfx->setFreeFont(&FreeSans9pt7b);
    gfx->setTextColor(C_CYAN, C_BLACK);
    gfx->setCursor(35, 90);
    gfx->print("v4.1 - Keira Style");
    gfx->setTextColor(C_DIMGRAY, C_BLACK);
    gfx->setCursor(15, 130);
    gfx->print("ESP32-S3 CyberBox");
    gfx->setCursor(15, 155);
    gfx->print("ST7789 240x240");
    gfx->setCursor(15, 180);
    gfx->print("BME280 + microSD");
    gfx->setTextColor(C_BORDERGRAY, C_BLACK);
    gfx->setCursor(50, 220);
    gfx->print("github.com/twve");
}

// ===================== PLACEHOLDER APP =====================
void PlaceholderApp::draw(TFT_eSprite* gfx) {
    gfx->fillSprite(C_BLACK);
    gfx->setFreeFont(&FreeSansBold12pt7b);
    gfx->setTextColor(C_DARKORANGE, C_BLACK);
    gfx->setCursor(10, H / 2 - 20);
    gfx->print(_name);
    gfx->setFreeFont(&FreeSans9pt7b);
    gfx->setTextColor(C_DIMGRAY, C_BLACK);
    gfx->setCursor(10, H / 2 + 10);
    gfx->print("Coming soon...");
}

// ===================== CORNER ACCENTS =====================
void drawCornerAccents(TFT_eSprite* gfx) {
    gfx->drawLine(0, 10, 0, 0, C_CORNER_CYAN);
    gfx->drawLine(0, 0, 10, 0, C_CORNER_CYAN);
    gfx->fillCircle(5, 5, 3, C_LED_GREEN);
    gfx->fillCircle(5, 5, 1, C_GREEN);
    gfx->drawLine(W - 1, 10, W - 1, 0, C_CORNER_CYAN);
    gfx->drawLine(W - 10, 0, W - 1, 0, C_CORNER_CYAN);
    gfx->drawLine(0, H - 11, 0, H - 1, C_CORNER_CYAN);
    gfx->drawLine(0, H - 1, 10, H - 1, C_CORNER_CYAN);
    gfx->drawLine(W - 1, H - 11, W - 1, H - 1, C_CORNER_CYAN);
    gfx->drawLine(W - 10, H - 1, W - 1, H - 1, C_CORNER_CYAN);
}

// ===================== SETUP =====================
void setup() {
    Serial.begin(115200); delay(100);
    Serial.println("=== LegacyOs v4.1 (Keira Architecture) ===");
    pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
    pinMode(BUZZ_PIN, OUTPUT); pinMode(PIN_BAT, INPUT);
    pinMode(PIN_PGOOD, INPUT_PULLUP); pinMode(PIN_CHG, INPUT_PULLUP);
    pinMode(SD_DET, INPUT_PULLUP);
    analogReadResolution(12);
    tft.init(); tft.setRotation(0); tft.fillScreen(TFT_BLACK);
    cv.createSprite(W, H);
    Wire.begin(I2C_SDA, I2C_SCL);
    SPIClass* sdSPI = new SPIClass(HSPI);
    sdSPI->begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, *sdSPI)) Serial.println("SD init failed");
    else Serial.println("SD OK");
    WiFi.mode(WIFI_STA);
    ctl.begin();
    initIcons();
    sndBoot();
    // Splash
    cv.fillSprite(C_BLACK);
    cv.setFreeFont(&FreeSansBold12pt7b);
    cv.setTextSize(2);
    cv.setTextColor(C_GREEN, C_BLACK);
    cv.setCursor(20, H / 2 - 30);
    cv.print("LEGACY OS");
    cv.setFreeFont(&FreeSans9pt7b);
    cv.setTextSize(1);
    cv.setTextColor(C_CYAN, C_BLACK);
    cv.setCursor(28, H / 2);
    cv.print("v4.1 - Keira Inspired");
    int bx = 30, by = H / 2 + 30, bw = W - 60, bh = 12;
    cv.drawRect(bx, by, bw, bh, C_DIMGRAY);
    for (int p = 0; p <= bw - 4; p += 4) {
        cv.fillRect(bx + 2, by + 2, p, bh - 4, C_GREEN);
        dispPush(); delay(8);
    }
    cv.fillRect(bx + 2, by + 2, bw - 4, bh - 4, C_GREEN);
    dispPush(); delay(300);
}

// ===================== LOOP =====================
void loop() {
    ctl.update();
    appMgr.update();
    if (alert.isActive()) {
        alert.update();
    } else if (appMgr.current()) {
        appMgr.current()->update();
        if (ctl.pressed(Controller::BACK)) {
            sndBack(); appMgr.back();
        }
    }
    cv.fillSprite(C_BLACK);
    if (appMgr.current()) appMgr.draw(&cv);
    drawStatusBar(&cv);
    drawCornerAccents(&cv);
    if (alert.isActive()) alert.draw(&cv);
    dispPush();
    delay(10);
}
