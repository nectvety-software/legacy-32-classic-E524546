#include "setup_manager.h"
#include "modbox_main.h"
#include <Preferences.h>
#include <WiFi.h>

Preferences settings;
SystemSettings systemSettings = {
    100, 30, 0, 0, 0, false, false, true, 0
};

#define MENU_ITEMS 9
const char* menuNames[MENU_ITEMS] = {
    "Brightness",
    "Dim Time",
    "Orientation",
    "UI Color",
    "UI Theme",
    "WiFi",
    "InstaBoot",
    "Beep Sound",
    "Startup App"
};

void SetupManager::begin() {
    loadSettings();
}

void SetupManager::loadSettings() {
    settings.begin("modbox", false);
    systemSettings.brightness = settings.getUChar("brightness", 100);
    systemSettings.dimTime = settings.getUChar("dimtime", 30);
    systemSettings.orientation = settings.getUChar("orient", 0);
    systemSettings.uiColor = settings.getUChar("uicolor", 0);
    systemSettings.uiTheme = settings.getUChar("uitheme", 0);
    systemSettings.wifiEnabled = settings.getBool("wifi", false);
    systemSettings.instaBoot = settings.getBool("instaboot", false);
    systemSettings.beepSound = settings.getBool("beep", true);
    systemSettings.startupApp = settings.getUChar("startapp", 0);
    settings.end();
}

void SetupManager::saveSettings() {
    settings.begin("modbox", false);
    settings.putUChar("brightness", systemSettings.brightness);
    settings.putUChar("dimtime", systemSettings.dimTime);
    settings.putUChar("orient", systemSettings.orientation);
    settings.putUChar("uicolor", systemSettings.uiColor);
    settings.putUChar("uitheme", systemSettings.uiTheme);
    settings.putBool("wifi", systemSettings.wifiEnabled);
    settings.putBool("instaboot", systemSettings.instaBoot);
    settings.putBool("beep", systemSettings.beepSound);
    settings.putUChar("startapp", systemSettings.startupApp);
    settings.end();
}

void SetupManager::resetDefaults() {
    systemSettings.brightness = 100;
    systemSettings.dimTime = 30;
    systemSettings.orientation = 0;
    systemSettings.uiColor = 0;
    systemSettings.uiTheme = 0;
    systemSettings.wifiEnabled = false;
    systemSettings.instaBoot = false;
    systemSettings.beepSound = true;
    systemSettings.startupApp = 0;
    saveSettings();
}

void SetupManager::applyBrightness() {
    uint8_t brightnessMap[5] = {1, 64, 128, 192, 255};
    uint8_t pwmVal = brightnessMap[systemSettings.brightness];
    analogWrite(TFT_BL, pwmVal);
}

void SetupManager::applyOrientation() {
    int rotations[5] = {0, 1, 3, 2, 0};
    gfx->setRotation(rotations[systemSettings.orientation]);
}

void SetupManager::applyWifi() {
    if (systemSettings.wifiEnabled) {
        WiFi.mode(WIFI_MODE_STA);
    } else {
        WiFi.disconnect();
        WiFi.mode(WIFI_MODE_NULL);
    }
}

const char* SetupManager::getBrightnessLabel(uint8_t val) {
    const char* labels[BRIGHTNESS_LEVELS] = {"1%", "25%", "50%", "75%", "100%"};
    if (val >= BRIGHTNESS_LEVELS) val = 0;
    return labels[val];
}

const char* SetupManager::getDimTimeLabel(uint8_t val) {
    const char* labels[DIM_TIME_OPTIONS] = {"10s", "20s", "30s", "60s", "Disabled"};
    if (val >= DIM_TIME_OPTIONS) val = 4;
    return labels[val];
}

const char* SetupManager::getOrientationLabel(uint8_t val) {
    const char* labels[ORIENTATION_OPTIONS] = {"Portrait 0", "Landscape 90", "Portrait 180", "Landscape 270", "Default"};
    if (val >= ORIENTATION_OPTIONS) val = 0;
    return labels[val];
}

const char* SetupManager::getUIColorLabel(uint8_t val) {
    const char* labels[UI_COLOR_OPTIONS] = {"Green", "Matrix", "Ble"};
    if (val >= UI_COLOR_OPTIONS) val = 0;
    return labels[val];
}

const char* SetupManager::getUIThemeLabel(uint8_t val) {
    const char* labels[UI_THEME_OPTIONS] = {"Green", "Matrix", "Ble"};
    if (val >= UI_THEME_OPTIONS) val = 0;
    return labels[val];
}

const char* SetupManager::getStartupAppLabel(uint8_t val) {
    const char* labels[STARTUP_APP_OPTIONS] = {"SubGHz", "LoRa Ra-01SH", "None"};
    if (val >= STARTUP_APP_OPTIONS) val = 0;
    return labels[val];
}

int setupCursor = 0;
int setupScrollOffset = 0;
bool setupEditMode = false;

void drawSetupMenu() {
    gfx->fillScreen(COLOR_BG);
    drawStatusBar();
    
    gfx->setTextSize(1);
    
    int maxShow = 9;
    int itemHeight = 28;
    int startY = 32;
    
    if (setupCursor > setupScrollOffset + maxShow - 1) {
        setupScrollOffset = setupCursor - maxShow + 1;
    }
    if (setupCursor < setupScrollOffset) {
        setupScrollOffset = setupCursor;
    }
    
    for (int i = 0; i < maxShow && (setupScrollOffset + i) < MENU_ITEMS; i++) {
        int idx = setupScrollOffset + i;
        int y = startY + i * itemHeight;
        
        if (idx == setupCursor) {
            gfx->fillRect(5, y, 225, itemHeight - 2, setupEditMode ? COLOR_GREEN : COLOR_GRAY);
            gfx->setTextColor(COLOR_BG);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        gfx->setCursor(10, y + 6);
        gfx->print(menuNames[idx]);
        
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(140, y + 6);
        
        SetupManager& sm = SetupManager::getInstance();
        
        switch (idx) {
            case 0: gfx->print(sm.getBrightnessLabel(systemSettings.brightness)); break;
            case 1: gfx->print(sm.getDimTimeLabel(systemSettings.dimTime)); break;
            case 2: gfx->print(sm.getOrientationLabel(systemSettings.orientation)); break;
            case 3: gfx->print(sm.getUIColorLabel(systemSettings.uiColor)); break;
            case 4: gfx->print(sm.getUIThemeLabel(systemSettings.uiTheme)); break;
            case 5: gfx->print(systemSettings.wifiEnabled ? "[ON]" : "[OFF]"); break;
            case 6: gfx->print(systemSettings.instaBoot ? "[ON]" : "[OFF]"); break;
            case 7: gfx->print(systemSettings.beepSound ? "[ON]" : "[OFF]"); break;
            case 8: gfx->print(sm.getStartupAppLabel(systemSettings.startupApp)); break;
        }
    }
    
    if (MENU_ITEMS > maxShow) {
        int barHeight = maxShow * itemHeight;
        int scrollBarY = startY + (setupScrollOffset * barHeight / MENU_ITEMS);
        int scrollBarH = maxShow * barHeight / MENU_ITEMS;
        if (scrollBarH < 10) scrollBarH = 10;
        gfx->fillRect(233, scrollBarY, 4, scrollBarH, COLOR_GRAY);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 305);
    gfx->print("UP/DOWN: Select | LEFT/RIGHT: Change | B: Back");
}

void handleSetupInput() {
    if (buttonPressed(KEY_UP)) {
        setupCursor--;
        if (setupCursor < 0) setupCursor = MENU_ITEMS - 1;
        setupEditMode = false;
        drawSetupMenu();
        delay(150);
        return;
    }
    
    if (buttonPressed(KEY_DOWN)) {
        setupCursor++;
        if (setupCursor >= MENU_ITEMS) setupCursor = 0;
        setupEditMode = false;
        drawSetupMenu();
        delay(150);
        return;
    }
    
    if (buttonPressed(KEY_RIGHT)) {
        setupEditMode = true;
        
        switch (setupCursor) {
            case 0:
                if (++systemSettings.brightness >= BRIGHTNESS_LEVELS) systemSettings.brightness = 0;
                SetupManager::getInstance().applyBrightness();
                break;
            case 1:
                if (++systemSettings.dimTime >= DIM_TIME_OPTIONS) systemSettings.dimTime = 0;
                break;
            case 2:
                if (++systemSettings.orientation >= ORIENTATION_OPTIONS) systemSettings.orientation = 0;
                SetupManager::getInstance().applyOrientation();
                break;
            case 3:
                if (++systemSettings.uiColor >= UI_COLOR_OPTIONS) systemSettings.uiColor = 0;
                break;
            case 4:
                if (++systemSettings.uiTheme >= UI_THEME_OPTIONS) systemSettings.uiTheme = 0;
                break;
            case 5:
                systemSettings.wifiEnabled = !systemSettings.wifiEnabled;
                SetupManager::getInstance().applyWifi();
                break;
            case 6:
                systemSettings.instaBoot = !systemSettings.instaBoot;
                break;
            case 7:
                systemSettings.beepSound = !systemSettings.beepSound;
                break;
            case 8:
                if (++systemSettings.startupApp >= STARTUP_APP_OPTIONS) systemSettings.startupApp = 0;
                break;
        }
        
        SetupManager::getInstance().saveSettings();
        drawSetupMenu();
        delay(150);
        return;
    }
    
    if (buttonPressed(KEY_LEFT)) {
        setupEditMode = true;
        
        switch (setupCursor) {
            case 0:
                if (systemSettings.brightness == 0) systemSettings.brightness = BRIGHTNESS_LEVELS - 1;
                else systemSettings.brightness--;
                SetupManager::getInstance().applyBrightness();
                break;
            case 1:
                if (systemSettings.dimTime == 0) systemSettings.dimTime = DIM_TIME_OPTIONS - 1;
                else systemSettings.dimTime--;
                break;
            case 2:
                if (systemSettings.orientation == 0) systemSettings.orientation = ORIENTATION_OPTIONS - 1;
                else systemSettings.orientation--;
                SetupManager::getInstance().applyOrientation();
                break;
            case 3:
                if (systemSettings.uiColor == 0) systemSettings.uiColor = UI_COLOR_OPTIONS - 1;
                else systemSettings.uiColor--;
                break;
            case 4:
                if (systemSettings.uiTheme == 0) systemSettings.uiTheme = UI_THEME_OPTIONS - 1;
                else systemSettings.uiTheme--;
                break;
            case 5:
                systemSettings.wifiEnabled = !systemSettings.wifiEnabled;
                SetupManager::getInstance().applyWifi();
                break;
            case 6:
                systemSettings.instaBoot = !systemSettings.instaBoot;
                break;
            case 7:
                systemSettings.beepSound = !systemSettings.beepSound;
                break;
            case 8:
                if (systemSettings.startupApp == 0) systemSettings.startupApp = STARTUP_APP_OPTIONS - 1;
                else systemSettings.startupApp--;
                break;
        }
        
        SetupManager::getInstance().saveSettings();
        drawSetupMenu();
        delay(150);
    }
}

void appSetup() {
    SetupManager::getInstance().loadSettings();
    setupCursor = 0;
    setupEditMode = false;
    
    drawSetupMenu();
    
    while (true) {
        handleSetupInput();
        
        if (buttonPressed(KEY_A)) {
            SetupManager::getInstance().saveSettings();
            break;
        }
        
        delay(10);
    }
}
