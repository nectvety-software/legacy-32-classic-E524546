#ifndef MAIN_MENU_APP_H
#define MAIN_MENU_APP_H

#include "../core/AppManager.h"

class MainMenuApp : public App {
public:
    const char* getName() override { return "Menu"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;

private:
    void draw();
    
    static const uint8_t MENU_COUNT = 12;
    const char* menuItems[MENU_COUNT] = {
        "WiFi", "Bluetooth", "Media", "Files",
        "Browser", "GPIO", "Lua", "Blender",
        "Calculator", "Notes", "Settings", "About"
    };
    
    AppID menuApps[MENU_COUNT] = {
        APP_WIFI, APP_BLUETOOTH, APP_MEDIA, APP_FILES,
        APP_BROWSER, APP_GPIO, APP_LUA, APP_BLENDER,
        APP_CALC, APP_NOTES, APP_SETTINGS, APP_ABOUT
    };
    
    uint8_t selectedIndex = 0;
    uint8_t topIndex = 0;
    uint8_t visibleItems;
};

#endif
