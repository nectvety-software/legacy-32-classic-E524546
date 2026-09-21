#ifndef NOKIA_OS_CONFIG_H
#define NOKIA_OS_CONFIG_H

#include <stdint.h>

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define KEY_UP       7
#define KEY_DOWN     46
#define KEY_LEFT     45
#define KEY_RIGHT    6
#define KEY_MENU     18
#define KEY_OPTION   8
#define KEY_SELECT   16
#define KEY_START    17
#define KEY_A        15
#define KEY_B        5

#define SD_CS        10
#define SD_MOSI      11
#define SD_SCK       13
#define SD_MISO      9

#define DEBOUNCE_DELAY 50
#define MENU_ANIMATION_SPEED 100

#define MAX_APPS 15
#define MAX_RECENT 5
#define MAX_FAVORITES 5

#define USE_SD_CARD
#define USE_WIFI
#define USE_BLE
#define USE_PSRAM

enum AppID {
    APP_NONE = 0,
    APP_WIFI,
    APP_BLUETOOTH,
    APP_MEDIA,
    APP_FILES,
    APP_BROWSER,
    APP_GPIO,
    APP_LUA,
    APP_BLENDER,
    APP_CALC,
    APP_NOTES,
    APP_SETTINGS,
    APP_ABOUT
};

struct AppInfo {
    const char* name;
    const char* icon;
    AppID id;
    bool requiresSD;
};

struct SystemState {
    bool wifiConnected;
    bool bleConnected;
    uint8_t batteryLevel;
    uint32_t totalRAM;
    uint32_t freeRAM;
    uint32_t uptime;
};

#endif
