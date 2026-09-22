#ifndef BLU_H
#define BLU_H

#include <Arduino.h>
#include "component/Display.h"
#include <vector>
#include <algorithm>
#include "component/Config.h"
#include "component/ui_utils.h"

// ==========================================
// THƯ VIỆN ĐIỀU KHIỂN BLE (Virtual Mouse & Keyboard)
// ==========================================
// Bỏ comment dòng dưới đây nếu bạn ĐÃ tải cài đặt thư viện BleCombo (T-vK)
// #define ENABLE_BLE_COMBO

#ifdef ENABLE_BLE_COMBO
#include <BleCombo.h>
#else
// Built-in Bluedroid BLE stack (arduino-esp32) used for the device scanner.
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#endif

// ==========================================
// COLOR PALETTE CYBERPUNK
// ==========================================
#define BLU_CYAN  0x07FF
#define BLU_PINK  0xFC18
#define BLU_RED   0xF800
#define BLU_DARK_BG TFT_BLACK
#define BLU_PANEL SymbianUI::SELECT
#define BLU_TEXT  0xFFFF
#define BLU_GLOW  0x03E0

extern TFT_eSPI tft;

// ==========================================
// STRUCT ANIMATION & UI
// ==========================================
struct Particle {
    float x, y;
    float vx, vy;
    int life;
    int maxLife;
    uint32_t color;
};

struct BluButton {
    int id; // KEY_UP, KEY_OPTION, v.v
    String label;
    int x, y, w, h;
    int shape; // 0 = Rect, 1 = Circle
    uint32_t color;
    float currentScale;
    float pulseAlpha;
    bool isPressed;
};

std::vector<Particle> bluParticles;
std::vector<BluButton> bluButtons;

TFT_eSprite* bluSprite = nullptr;

String lastCmd = "READY";
unsigned long lastActivityTime = 0;
bool isSleeping = false;
int btState = 0; // 0 = Disconnected, 1 = Connected

int mouseSpeed = 2;
bool isMouseMode = true;

enum BluScreen { BLU_MAIN_SCREEN, BLU_SCAN_SCREEN, BLU_DEVICE_SCREEN,
                 BLU_REMOTE_SCREEN, BLU_STATUS_SCREEN, BLU_ABOUT_SCREEN };
BluScreen bluScreen = BLU_MAIN_SCREEN;
int bluMenuSel = 0;

// ==========================================
// BLE SCANNER (name / MAC / RSSI) + connect
// ==========================================
// Scans nearby BLE advertisers, lists them with signal strength, and lets the
// user connect to inspect the advertised GATT services (Bruce-style scanner).
struct BleFound {
    String name;
    String addr;
    int rssi;
};
std::vector<BleFound> bleFound;
int bleSel = 0;              // highlighted row in the scan list
int bleScroll = 0;          // scroll offset in the scan list
bool bleInited = false;     // BLEDevice::init() has been called
String bleDeviceInfo = "";  // newline-separated service UUIDs for detail view
String bleConnState = "";   // connection status text for detail view

void drawBluProfessionalUI() {
    if (bluScreen == BLU_MAIN_SCREEN) {
        SymbianUI::drawChrome("Bluetooth", "Select", "Back");
        const char* labels[] = {"Scan Devices", "Remote Control", "Device Status", "About"};
        const SymbianUI::Icon icons[] = {
            SymbianUI::ICON_BLUETOOTH, SymbianUI::ICON_SYSTEM,
            SymbianUI::ICON_INFO, SymbianUI::ICON_INFO
        };
        for (int i = 0; i < 4; ++i) {
            String value = ">";
            if (i == 0) {
                value = bleFound.empty() ? ">" : (String(bleFound.size()) + " found");
            } else if (i == 1) value = isMouseMode ? "Mouse" : "Keyboard";
            SymbianUI::drawListRow(58 + i * 38, 34, labels[i], i == bluMenuSel,
                                   value, icons[i]);
        }
        SymbianUI::drawSectionLabel(216, "Host connection");
#ifdef ENABLE_BLE_COMBO
        SymbianUI::drawInfoLine(246, "State", btState ? "Connected" : "Waiting for host");
#else
        SymbianUI::drawInfoLine(246, "State", "BLE HID disabled");
#endif
        SymbianUI::drawInfoLine(265, "Name", "POCHITA OS");
        return;
    }

    if (bluScreen == BLU_REMOTE_SCREEN) {
        SymbianUI::drawChrome(isMouseMode ? "Bluetooth Mouse" : "Bluetooth Keyboard",
                              "Mode", "Back");
        SymbianUI::drawSectionLabel(57, "Physical key mapping");
        SymbianUI::drawInfoLine(86, "D-pad", isMouseMode ? "Move pointer" : "Arrow keys");
        SymbianUI::drawInfoLine(108, "A", isMouseMode ? "Left click" : "Enter");
        SymbianUI::drawInfoLine(130, "B", isMouseMode ? "Right click" : "Backspace");
        SymbianUI::drawInfoLine(152, "Select", isMouseMode ? "Middle click" : "Space");
        SymbianUI::drawInfoLine(174, "Option", "Switch mode");
        SymbianUI::drawSectionLabel(204, "Last command");
        SymbianUI::drawInfoLine(234, "Action", lastCmd);
#ifdef ENABLE_BLE_COMBO
        SymbianUI::drawInfoLine(256, "Link", btState ? "Connected" : "Waiting");
#else
        SymbianUI::drawInfoLine(256, "Link", "Library required");
#endif
        return;
    }

    if (bluScreen == BLU_STATUS_SCREEN) {
        SymbianUI::drawChrome("Bluetooth Status", "", "Back");
#ifdef ENABLE_BLE_COMBO
        SymbianUI::drawEmptyState(SymbianUI::ICON_BLUETOOTH,
                                  btState ? "Connected" : "Waiting for host",
                                  "Pair POCHITA OS from the host");
#else
        SymbianUI::drawEmptyState(SymbianUI::ICON_BLUETOOTH,
                                  "BLE HID unavailable",
                                  "Enable BleCombo to use remote control");
#endif
        SymbianUI::drawInfoLine(220, "Mode", isMouseMode ? "Mouse" : "Keyboard");
        SymbianUI::drawInfoLine(242, "Device", "ESP32-S3");
        return;
    }

    SymbianUI::drawChrome("About Bluetooth", "", "Back");
    SymbianUI::drawEmptyState(SymbianUI::ICON_INFO, "Bluetooth Remote",
                              "Physical-key HID controller");
    SymbianUI::drawInfoLine(220, "System", "POCHITA OS");
    SymbianUI::drawInfoLine(242, "Hardware", "ESP32-S3");
}

// ==========================================
// BLE SCANNER SCREENS + LOGIC
// ==========================================
void drawBluScanScreen() {
    SymbianUI::drawChrome("BLE Devices", "Connect", "Back");
    if (bleFound.empty()) {
        SymbianUI::drawEmptyState(SymbianUI::ICON_BLUETOOTH,
                                  "No devices found", "Press Option to rescan");
        SymbianUI::drawSoftkeys("Rescan", "Back");
        return;
    }
    const int itemH = 40, gap = 2, visible = 5;
    for (int i = 0; i < visible; i++) {
        int idx = bleScroll + i;
        if (idx >= (int)bleFound.size()) break;
        int y = 56 + i * (itemH + gap);
        bool sel = (idx == bleSel);
        String label = UiLayout::ellipsize(bleFound[idx].name, 22);
        String value = String(bleFound[idx].rssi) + " dBm";
        SymbianUI::drawListRow(y, itemH, label, sel, value, SymbianUI::ICON_BLUETOOTH);
        // MAC address subtitle underneath the device name.
        tft.setTextColor(sel ? TFT_WHITE : SymbianUI::DIM,
                         sel ? SymbianUI::SELECT : SymbianUI::BG);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(bleFound[idx].addr, 40, y + 28, 1);
    }
    tft.setTextColor(TFT_SILVER, SymbianUI::BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(String(bleFound.size()) + " devices", 232, 286, 1);
    SymbianUI::drawSoftkeys("Connect", "Back");
}

void drawBluDeviceScreen() {
    SymbianUI::drawChrome("Device", "", "Back");
    String name = (bleSel < (int)bleFound.size()) ? bleFound[bleSel].name : "";
    String addr = (bleSel < (int)bleFound.size()) ? bleFound[bleSel].addr : "";
    SymbianUI::drawSectionLabel(58, UiLayout::ellipsize(name, 28));
    SymbianUI::drawInfoLine(88, "MAC", addr);
    SymbianUI::drawInfoLine(110, "RSSI",
        (bleSel < (int)bleFound.size()) ? String(bleFound[bleSel].rssi) + " dBm" : "-");
    SymbianUI::drawInfoLine(132, "State", bleConnState);
    SymbianUI::drawSectionLabel(160, "Services");

    tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
    tft.setTextDatum(TL_DATUM);
    int y = 186;
    String info = bleDeviceInfo;
    while (info.length() && y < 282) {
        int nl = info.indexOf('\n');
        String line = (nl == -1) ? info : info.substring(0, nl);
        tft.drawString(UiLayout::ellipsize(line, 34), 12, y, 1);
        if (nl == -1) break;
        info = info.substring(nl + 1);
        y += 16;
    }
    SymbianUI::drawSoftkeys("", "Back");
}

// Initialise/tear down the built-in BLE stack on demand to reclaim RAM.
void bleEnsureInit() {
#ifndef ENABLE_BLE_COMBO
    if (!bleInited) {
        BLEDevice::init("");
        bleInited = true;
    }
#endif
}

void bleDeinit() {
#ifndef ENABLE_BLE_COMBO
    if (bleInited) {
        BLEDevice::deinit(true);
        bleInited = false;
    }
#endif
}

// Perform a blocking BLE scan and populate bleFound (sorted by RSSI).
void bleScanNow() {
    bleFound.clear();
    bleSel = 0;
    bleScroll = 0;
#ifndef ENABLE_BLE_COMBO
    SymbianUI::drawMessageScreen("BLE Scanner", SymbianUI::ICON_BLUETOOTH,
                                 "Scanning", "Searching for BLE devices", "", "Cancel");
    SymbianUI::drawProgressBar(35, 188, 170, 30);
    bleEnsureInit();
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    SymbianUI::drawProgressBar(35, 188, 170, 60);
    BLEScanResults results = scan->start(4, false);
    SymbianUI::drawProgressBar(35, 188, 170, 100);
    int cnt = results.getCount();
    for (int i = 0; i < cnt; ++i) {
        BLEAdvertisedDevice d = results.getDevice(i);
        BleFound f;
        f.name = d.haveName() ? String(d.getName().c_str()) : String("(no name)");
        f.addr = String(d.getAddress().toString().c_str());
        f.rssi = d.getRSSI();
        bleFound.push_back(f);
    }
    scan->clearResults();
    std::sort(bleFound.begin(), bleFound.end(),
              [](const BleFound &a, const BleFound &b) { return a.rssi > b.rssi; });
#endif
    bluScreen = BLU_SCAN_SCREEN;
    drawBluScanScreen();
}

// Connect to the selected device and list its advertised GATT services.
void bleConnectTo(int idx) {
    if (idx < 0 || idx >= (int)bleFound.size()) return;
    bleDeviceInfo = "";
    bleConnState = "Connecting...";
    bluScreen = BLU_DEVICE_SCREEN;
    drawBluDeviceScreen();
#ifndef ENABLE_BLE_COMBO
    bleEnsureInit();
    BLEClient* client = BLEDevice::createClient();
    bool ok = client->connect(BLEAddress(bleFound[idx].addr.c_str()));
    if (ok) {
        bleConnState = "Connected";
        std::map<std::string, BLERemoteService*>* services = client->getServices();
        int n = 0;
        for (auto &kv : *services) {
            if (n >= 6) { bleDeviceInfo += "..."; break; }
            bleDeviceInfo += String(kv.first.c_str()) + "\n";
            n++;
        }
        if (n == 0) bleDeviceInfo = "No services";
        client->disconnect();
    } else {
        bleConnState = "Failed";
        bleDeviceInfo = "Could not connect";
    }
    // Note: this arduino-esp32 BLE version has no BLEDevice::deleteClient(); the
    // whole stack (and all clients) is released by bleDeinit() on app exit.
#else
    bleConnState = "Unavailable";
    bleDeviceInfo = "BLE HID mode active";
#endif
    drawBluDeviceScreen();
}

// ==========================================
// KHỞI TẠO NÚT BẤM
// ==========================================
void initBluButtons() {
    bluButtons.clear();
    // D-Pad
    bluButtons.push_back({KEY_UP,    "^",  120, 25,  40, 34, 0, BLU_PANEL, 1.0f, 0, false});
    bluButtons.push_back({KEY_DOWN,  "v",  120, 91, 40, 34, 0, BLU_PANEL, 1.0f, 0, false});
    bluButtons.push_back({KEY_LEFT,  "<",  78,  58,  40, 34, 0, BLU_PANEL, 1.0f, 0, false});
    bluButtons.push_back({KEY_RIGHT, ">",  162, 58,  40, 34, 0, BLU_PANEL, 1.0f, 0, false});
    
    // Action Buttons
    bluButtons.push_back({KEY_A, "B", 55, 137, 42, 42, 1, BLU_PANEL, 1.0f, 0, false});
    bluButtons.push_back({KEY_OPTION, "A", 185, 137, 42, 42, 1, BLU_PANEL, 1.0f, 0, false});
    
    // Bottom Controls
    bluButtons.push_back({KEY_SELECT,   "MODE",  45,  190, 60, 24, 0, BLU_PANEL, 1.0f, 0, false});
    bluButtons.push_back({KEY_START, "CLICK", 120, 190, 60, 24, 0, BLU_PANEL, 1.0f, 0, false});
    bluButtons.push_back({KEY_MENU, "MODE",  195, 190, 60, 24, 0, BLU_PANEL, 1.0f, 0, false});
}

// ==========================================
// PARTICLE SYSTEM
// ==========================================
void spawnParticles(int x, int y, uint32_t color) {
    for(int i=0; i<8; i++) {
        Particle p;
        p.x = x; p.y = y;
        p.vx = (random(-20, 20) / 10.0f);
        p.vy = (random(-20, 20) / 10.0f);
        p.maxLife = random(15, 30);
        p.life = p.maxLife;
        p.color = color;
        bluParticles.push_back(p);
    }
}

void updateParticles() {
    for (int i = bluParticles.size() - 1; i >= 0; i--) {
        bluParticles[i].x += bluParticles[i].vx;
        bluParticles[i].y += bluParticles[i].vy;
        bluParticles[i].life--;
        if (bluParticles[i].life <= 0) {
            bluParticles.erase(bluParticles.begin() + i);
        }
    }
}

void drawParticles() {
    for (auto& p : bluParticles) {
        if (p.life > 0) {
            int r = p.life > (p.maxLife / 2) ? 2 : 1;
            bluSprite->fillCircle(p.x, p.y, r, p.color);
        }
    }
}

// ==========================================
// RENDER UI MƯỢT MÀ BẰNG SPRITE 
// ==========================================
void drawBluButton(BluButton& btn) {
    if (btn.currentScale <= 0.01f) return;
    
    int drawW = (int)(btn.w * btn.currentScale);
    int drawH = (int)(btn.h * btn.currentScale);
    int drx = btn.x - (drawW / 2);
    int dry = btn.y - (drawH / 2);
    
    // Draw Glow Pulse (Shadow / Aura)
    if (btn.isPressed || btn.pulseAlpha > 0.1f) {
        int glowPadding = 4 + (int)(btn.pulseAlpha * 6.0f);
        // Fade trick: Draw borders
        if (btn.shape == 0) {
            for(int g=1; g<=glowPadding; g++) {
                bluSprite->drawRoundRect(drx - g, dry - g, drawW + g*2, drawH + g*2, 8, BLU_GLOW);
            }
        } else {
            for(int g=1; g<=glowPadding; g++) {
                bluSprite->drawCircle(btn.x, btn.y, (drawW/2) + g, BLU_GLOW);
            }
        }
    }

    // 3D Bevel Body
    if (btn.shape == 0) {
        bluSprite->fillRoundRect(drx, dry, drawW, drawH, 6, btn.color);
        // Bevel Hightlight Soft
        bluSprite->drawRoundRect(drx, dry, drawW, drawH, 6, TFT_WHITE);
    } else {
        bluSprite->fillCircle(btn.x, btn.y, drawW / 2, btn.color);
        bluSprite->drawCircle(btn.x, btn.y, drawW / 2, TFT_WHITE);
    }

    // Text Label
    bluSprite->setTextColor(BLU_TEXT);
    bluSprite->setTextDatum(MC_DATUM);
    bluSprite->drawString(btn.label, btn.x, btn.y, (btn.h > 30 ? 2 : 1));
}

void drawBluHeader() {
    SymbianUI::drawStatusBar();
    SymbianUI::drawTitle(isMouseMode ? "Bluetooth mouse" : "Bluetooth keyboard");
}

void drawBluFooter() {
    tft.fillRect(0, 270, UiLayout::WIDTH, 26, TFT_BLACK);
    tft.drawFastHLine(0, 270, UiLayout::WIDTH, SymbianUI::DIVIDER);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    #ifdef ENABLE_BLE_COMBO
    String state = btState ? "Connected" : "Waiting for BLE host";
    #else
    String state = "BLE HID library not enabled";
    #endif
    tft.drawString(state + " | " + lastCmd, 5, 283, 1);
    SymbianUI::drawSoftkeys("Mode", "Home");
}

void renderBluApp() {
    if (bluSprite == nullptr) {
        bluSprite = new TFT_eSprite(&tft);
        // Tạo sprite 240x270 cho vùng hiển thị animation
        if(!bluSprite->createSprite(240, 212)) {
            tft.fillScreen(TFT_RED);
            tft.drawString("RAM NOT ENOUGH", 120, 160, 2);
            return;
        }
    }
    
    bluSprite->fillSprite(BLU_DARK_BG); 
    
    // Gradient Background giả lập: Lưới / chấm bi cyberpunk?
    for(int i=0; i<240; i+=20) {
        bluSprite->drawLine(i, 0, i, 212, 0x1082);
    }
    for(int j=0; j<212; j+=20) {
        bluSprite->drawLine(0, j, 240, j, 0x1082);
    }

    // Vẽ Buttons
    for (auto& btn : bluButtons) {
        // Animation Logic: Hover Glow & Elastic Scale
        if (btn.isPressed) {
            float target = 0.85f;
            btn.currentScale += (target - btn.currentScale) * 0.4f;
            btn.pulseAlpha = 1.0f;
        } else {
            float target = 1.0f;
            btn.currentScale += (target - btn.currentScale) * 0.4f;
            btn.pulseAlpha *= 0.8f; // Fade out glow
        }
        drawBluButton(btn);
    }

    updateParticles();
    drawParticles();

    // Push Sprite
    bluSprite->pushSprite(0, SymbianUI::HEADER_H);
}

// ==========================================
// BLE LOGIC & LOOP
// ==========================================
void initBluApp() {
    initBluButtons();
    bluScreen = BLU_MAIN_SCREEN;
    bluMenuSel = 0;
    drawBluProfessionalUI();
    lastActivityTime = millis();
    isSleeping = false;
    
#ifdef ENABLE_BLE_COMBO
    Keyboard.begin();
    Mouse.begin();
#endif
}

void execBluCommand(int btnId, BluButton& btn) {
    lastActivityTime = millis();
    if(isSleeping) {
        isSleeping = false;
        tft.fillScreen(BLU_DARK_BG);
        drawBluHeader();
        drawBluFooter();
        return; 
    }

    btn.isPressed = true;
    spawnParticles(btn.x, btn.y, btn.color);
    lastCmd = btn.label;
    drawBluFooter();
    
#ifdef ENABLE_BLE_COMBO
    btState = (Keyboard.isConnected() || Mouse.isConnected()) ? 1 : 0;
#else
    btState = 0; 
#endif
    drawBluHeader();

    // Mode Toggle
    if (btnId == KEY_OPTION || btnId == KEY_SELECT) {
        isMouseMode = !isMouseMode;
        lastCmd = isMouseMode ? "MODE: MOUSE" : "MODE: KEYBOARD";
        drawBluHeader(); drawBluFooter();
    }

#ifdef ENABLE_BLE_COMBO
    if (btState) {
        if (isMouseMode) {
            if (btnId == KEY_UP) Mouse.move(0, -mouseSpeed * 5);
            else if (btnId == KEY_DOWN) Mouse.move(0, mouseSpeed * 5);
            else if (btnId == KEY_LEFT) Mouse.move(-mouseSpeed * 5, 0);
            else if (btnId == KEY_RIGHT) Mouse.move(mouseSpeed * 5, 0);
            else if (btnId == KEY_OPTION) Mouse.click(MOUSE_LEFT);
            else if (btnId == KEY_A) Mouse.click(MOUSE_RIGHT);
            else if (btnId == KEY_START) Mouse.click(MOUSE_MIDDLE);
        } else {
            if (btnId == KEY_UP) Keyboard.write(KEY_UP_ARROW);
            else if (btnId == KEY_DOWN) Keyboard.write(KEY_DOWN_ARROW);
            else if (btnId == KEY_LEFT) Keyboard.write(KEY_LEFT_ARROW);
            else if (btnId == KEY_RIGHT) Keyboard.write(KEY_RIGHT_ARROW);
            else if (btnId == KEY_OPTION) Keyboard.write(KEY_RETURN);
            else if (btnId == KEY_A) Keyboard.write(KEY_BACKSPACE);
            else if (btnId == KEY_START) Keyboard.write(' ');
            else if (btnId == KEY_B) Keyboard.write(KEY_MEDIA_PLAY_PAUSE);
        }
    }
#endif
}

void loopBluLegacy() {
    // 1. Quản lý Auto-Sleep (30s rảnh)
    if (!isSleeping && (millis() - lastActivityTime > 30000)) {
        isSleeping = true;
        tft.fillScreen(TFT_BLACK);
        if(bluSprite) {
            bluSprite->deleteSprite();
            delete bluSprite;
            bluSprite = nullptr;
        }
        return;
    }
    if (isSleeping) {
        for (auto& btn : bluButtons) {
            if (digitalRead(btn.id) == LOW) {
                initBluApp(); 
                delay(300); 
                break;
            }
        }
        return; 
    }

    // 2. Xử lý logic phím vật lý
    bool anyPressed = false;
    for (auto& btn : bluButtons) {
        if (digitalRead(btn.id) == LOW) {
            if (!btn.isPressed) {
                execBluCommand(btn.id, btn);
            }
            btn.isPressed = true;
            anyPressed = true;
        } else {
            btn.isPressed = false;
        }
    }
    
    // Thoát ra Router: Ấn giữ START hoặc BACK. Ở đây ta dùng START ấn giữ
    static unsigned long homeHoldTime = 0;
    if (digitalRead(KEY_MENU) == LOW) {
        if (homeHoldTime == 0) homeHoldTime = millis();
        else if (millis() - homeHoldTime > 1500) {
            // EXIT
            extern SystemMode currentMode;
            extern void drawLauncherContent();
            
            currentMode = MODE_LAUNCHER; 
            drawLauncherContent();
            
            if(bluSprite) {
                bluSprite->deleteSprite();
                delete bluSprite;
                bluSprite = nullptr;
            }
            return;
        }
    } else {
        homeHoldTime = 0;
    }
    
    // 3. Render đồ họa 60 FPS (Không Delay!)
    renderBluApp();
    
    // Limit Frame Rate ~60FPS để ESP BLE loop chạy mượt
    delay(16); 
}

void loopBlu() {
    // --- BLE scan list ------------------------------------------------------
    if (bluScreen == BLU_SCAN_SCREEN) {
        if (isBackPressed()) {
            bluScreen = BLU_MAIN_SCREEN;
            drawBluProfessionalUI();
            delay(180);
            return;
        }
        if (digitalRead(KEY_OPTION) == LOW) {   // rescan
            bleScanNow();
            delay(200);
            return;
        }
        if (bleFound.empty()) { delay(16); return; }
        if (buttonManager.isJustPressed(KEY_DOWN)) {
            if (bleSel < (int)bleFound.size() - 1) {
                bleSel++;
                if (bleSel >= bleScroll + 5) bleScroll++;
                drawBluScanScreen();
            }
            delay(150);
            return;
        }
        if (buttonManager.isJustPressed(KEY_UP)) {
            if (bleSel > 0) {
                bleSel--;
                if (bleSel < bleScroll) bleScroll--;
                drawBluScanScreen();
            }
            delay(150);
            return;
        }
        if (isSelectPressed()) {
            bleConnectTo(bleSel);
            delay(200);
            return;
        }
        delay(16);
        return;
    }

    // --- BLE device detail --------------------------------------------------
    if (bluScreen == BLU_DEVICE_SCREEN) {
        if (isBackPressed()) {
            bluScreen = BLU_SCAN_SCREEN;
            drawBluScanScreen();
            delay(180);
        }
        delay(16);
        return;
    }

    // --- Remote HID control screen -----------------------------------------
    if (bluScreen == BLU_REMOTE_SCREEN) {
        if (isBackPressed()) {
            bluScreen = BLU_MAIN_SCREEN;
            drawBluProfessionalUI();
            delay(180);
            return;
        }
        bool anyPressed = false;
        for (auto& btn : bluButtons) {
            if (digitalRead(btn.id) == LOW) {
                if (!btn.isPressed) execBluCommand(btn.id, btn);
                btn.isPressed = true;
                anyPressed = true;
            } else {
                btn.isPressed = false;
            }
        }
        static unsigned long lastStatusRefresh = 0;
        if (anyPressed || millis() - lastStatusRefresh > 1000) {
            drawBluProfessionalUI();
            lastStatusRefresh = millis();
        }
        delay(16);
        return;
    }

    // --- Main / Status / About menus ---------------------------------------
    if (isBackPressed()) {
        if (bluScreen == BLU_MAIN_SCREEN) {
            extern SystemMode currentMode;
            extern void drawLauncherContent();
            bleDeinit();
            currentMode = MODE_LAUNCHER;
            drawLauncherContent();
        } else {
            bluScreen = BLU_MAIN_SCREEN;
            drawBluProfessionalUI();
        }
        delay(180);
        return;
    }

    if (bluScreen == BLU_MAIN_SCREEN) {
        if (buttonManager.isJustPressed(KEY_DOWN)) {
            bluMenuSel = (bluMenuSel + 1) % 4;
            drawBluProfessionalUI();
            delay(150);
        } else if (buttonManager.isJustPressed(KEY_UP)) {
            bluMenuSel = (bluMenuSel + 3) % 4;
            drawBluProfessionalUI();
            delay(150);
        } else if (isSelectPressed()) {
            if (bluMenuSel == 0) {
                bleScanNow();                    // BLE scan + device list
            } else if (bluMenuSel == 1) {
                bluScreen = BLU_REMOTE_SCREEN;
                drawBluProfessionalUI();
            } else if (bluMenuSel == 3) {
                bluScreen = BLU_ABOUT_SCREEN;
                drawBluProfessionalUI();
            } else {
                bluScreen = BLU_STATUS_SCREEN;
                drawBluProfessionalUI();
            }
            delay(180);
        }
    }
    delay(16);
}

#endif
