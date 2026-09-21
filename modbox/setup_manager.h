#ifndef SETUP_MANAGER_H
#define SETUP_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#define BRIGHTNESS_LEVELS 5
#define DIM_TIME_OPTIONS 5
#define ORIENTATION_OPTIONS 5
#define UI_COLOR_OPTIONS 3
#define UI_THEME_OPTIONS 3
#define STARTUP_APP_OPTIONS 3

struct SystemSettings {
    uint8_t brightness;
    uint8_t dimTime;
    uint8_t orientation;
    uint8_t uiColor;
    uint8_t uiTheme;
    bool wifiEnabled;
    bool instaBoot;
    bool beepSound;
    uint8_t startupApp;
};

extern SystemSettings systemSettings;
extern Preferences settings;

class SetupManager {
public:
    static SetupManager& getInstance() {
        static SetupManager instance;
        return instance;
    }
    
    void begin();
    void saveSettings();
    void loadSettings();
    void resetDefaults();
    void applyBrightness();
    void applyOrientation();
    void applyWifi();
    
    const char* getBrightnessLabel(uint8_t val);
    const char* getDimTimeLabel(uint8_t val);
    const char* getOrientationLabel(uint8_t val);
    const char* getUIColorLabel(uint8_t val);
    const char* getUIThemeLabel(uint8_t val);
    const char* getStartupAppLabel(uint8_t val);
    
private:
    SetupManager() {}
    ~SetupManager() {}
    
    SetupManager(const SetupManager&) = delete;
    SetupManager& operator=(const SetupManager&) = delete;
};

void appSetup();
void setupDrawMenu();
void setupInputHandler();

#endif
