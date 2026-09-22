#include "icon_manager.h"
#include <SPIFFS.h>
#include <FS.h>

// Static icon arrays - will be populated from loaded data
uint16_t epd_bitmap_wifi[ICON_SIZE];
uint16_t epd_bitmap_sd[ICON_SIZE];
uint16_t epd_bitmap_paint[ICON_SIZE];
uint16_t epd_bitmap_ble[ICON_SIZE];
uint16_t epd_bitmap_terminal[ICON_SIZE];
uint16_t epd_bitmap_web[ICON_SIZE];
uint16_t epd_bitmap_script[ICON_SIZE];
uint16_t epd_bitmap_retro[ICON_SIZE];
uint16_t epd_bitmap_setup[ICON_SIZE];

uint8_t* iconData = nullptr;
int iconCount = 0;
bool iconsLoaded = false;

bool IconManager::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS init failed!");
        return false;
    }
    Serial.println("SPIFFS initialized");
    return true;
}

void IconManager::populateStaticArrays() {
    if (!iconData || iconCount == 0) return;
    
    // Copy loaded icon data to static arrays
    if (iconCount > 0) memcpy(epd_bitmap_wifi, iconData + 0 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 1) memcpy(epd_bitmap_sd, iconData + 1 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 2) memcpy(epd_bitmap_paint, iconData + 2 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 3) memcpy(epd_bitmap_ble, iconData + 3 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 4) memcpy(epd_bitmap_terminal, iconData + 4 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 5) memcpy(epd_bitmap_web, iconData + 5 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 6) memcpy(epd_bitmap_script, iconData + 6 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 7) memcpy(epd_bitmap_retro, iconData + 7 * ICON_SIZE * 2, ICON_SIZE * 2);
    if (iconCount > 8) memcpy(epd_bitmap_setup, iconData + 8 * ICON_SIZE * 2, ICON_SIZE * 2);
    
    Serial.println("Static icon arrays populated");
}

bool IconManager::loadIcons(const char* filename) {
    if (!SPIFFS.exists(filename)) {
        Serial.println("Icon file not found: " + String(filename));
        return false;
    }
    
    File file = SPIFFS.open(filename, "rb");
    if (!file) {
        Serial.println("Failed to open icon file");
        return false;
    }
    
    size_t fileSize = file.size();
    iconCount = fileSize / (ICON_WIDTH * ICON_HEIGHT * 2);
    
    if (iconCount > MAX_ICONS) {
        iconCount = MAX_ICONS;
    }
    
    freeIcons();
    iconData = (uint8_t*)malloc(iconCount * ICON_WIDTH * ICON_HEIGHT * 2);
    
    if (!iconData) {
        Serial.println("Failed to allocate memory for icons");
        iconCount = 0;
        file.close();
        return false;
    }
    
    size_t bytesRead = file.read(iconData, iconCount * ICON_WIDTH * ICON_HEIGHT * 2);
    file.close();
    
    if (bytesRead != (size_t)(iconCount * ICON_WIDTH * ICON_HEIGHT * 2)) {
        Serial.println("Warning: Read " + String(bytesRead) + " bytes, expected " + String(iconCount * ICON_WIDTH * ICON_HEIGHT * 2));
    }
    
    // Populate static arrays with loaded data
    populateStaticArrays();
    
    iconsLoaded = true;
    Serial.println("Loaded " + String(iconCount) + " icons and populated static arrays");
    return true;
}

const uint16_t* IconManager::getIcon(int index) {
    if (!iconsLoaded || index < 0 || index >= iconCount) {
        return nullptr;
    }
    return (const uint16_t*)(iconData + index * ICON_WIDTH * ICON_HEIGHT * 2);
}

const uint16_t* IconManager::getIconByName(const char* name) {
    if (strcmp(name, "wifi") == 0) return epd_bitmap_wifi;
    if (strcmp(name, "sd") == 0) return epd_bitmap_sd;
    if (strcmp(name, "paint") == 0) return epd_bitmap_paint;
    if (strcmp(name, "ble") == 0) return epd_bitmap_ble;
    if (strcmp(name, "terminal") == 0) return epd_bitmap_terminal;
    if (strcmp(name, "web") == 0) return epd_bitmap_web;
    if (strcmp(name, "script") == 0) return epd_bitmap_script;
    if (strcmp(name, "retro") == 0) return epd_bitmap_retro;
    if (strcmp(name, "setup") == 0) return epd_bitmap_setup;
    return nullptr;
}

void IconManager::freeIcons() {
    if (iconData) {
        free(iconData);
        iconData = nullptr;
    }
    iconsLoaded = false;
    iconCount = 0;
}
