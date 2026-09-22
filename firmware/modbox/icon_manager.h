#ifndef ICON_MANAGER_H
#define ICON_MANAGER_H

#include <Arduino.h>

////////////// Icon Config //////////////
#define ICON_WIDTH 48
#define ICON_HEIGHT 48
#define ICON_SIZE (ICON_WIDTH * ICON_HEIGHT)
#define MAX_ICONS 15

////////////// Icon Index //////////////
#define ICON_WIFI   0
#define ICON_WEB    1
#define ICON_SETUP  2
#define ICON_RETRO  3
#define ICON_SCRIPT 4
#define ICON_BLE    5
#define ICON_PAINT  6
#define ICON_SD     7
#define ICON_TERMINAL 8

////////////// Icon Bitmaps (defined in icon_manager.cpp) //////////////
extern uint16_t epd_bitmap_wifi[ICON_SIZE];
extern uint16_t epd_bitmap_sd[ICON_SIZE];
extern uint16_t epd_bitmap_paint[ICON_SIZE];
extern uint16_t epd_bitmap_ble[ICON_SIZE];
extern uint16_t epd_bitmap_terminal[ICON_SIZE];
extern uint16_t epd_bitmap_web[ICON_SIZE];
extern uint16_t epd_bitmap_script[ICON_SIZE];
extern uint16_t epd_bitmap_retro[ICON_SIZE];
extern uint16_t epd_bitmap_setup[ICON_SIZE];

////////////// Icon Manager //////////////
class IconManager {
public:
    static IconManager& getInstance() {
        static IconManager instance;
        return instance;
    }
    
    bool begin();
    bool loadIcons(const char* filename);
    const uint16_t* getIcon(int index);
    const uint16_t* getIconByName(const char* name);
    int getIconCount() { return MAX_ICONS; }
    bool hasIcons() { return true; }
    void freeIcons();
    
private:
    void populateStaticArrays();
    IconManager() {}
    ~IconManager() {}
    
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;
};

////////////// Macros //////////////
#define LOAD_ICONS_FROM_SPIFFS(filename) IconManager::getInstance().loadIcons(filename)
#define GET_ICON(index) IconManager::getInstance().getIcon(index)
#define GET_ICON_BY_NAME(name) IconManager::getInstance().getIconByName(name)
#define HAS_ICONS() IconManager::getInstance().hasIcons()
#define GET_ICON_COUNT() IconManager::getInstance().getIconCount()

#endif
