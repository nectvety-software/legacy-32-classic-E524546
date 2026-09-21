#ifndef UI_EXAMPLE_H
#define UI_EXAMPLE_H

#include "ui_interface.h"
#include "icon_manager.h"
#include "assets/icons_bitmap.h"

//////////////// EXAMPLE UI LAYOUTS //////////////////

// Example 1: Simple App Grid
void createAppGrid() {
    MenuItem appItems[] = {
        {"WiFi", epd_bitmap_wifi, ICON_WIFI, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("WiFi Menu");
        }, true},
        
        {"BLE", epd_bitmap_ble, ICON_BLE, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("BLE Menu");
        }, true},
        
        {"Web", epd_bitmap_web, ICON_WEB, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("Web Server");
        }, true},
        
        {"SD", epd_bitmap_sd, ICON_SD, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("SD Card");
        }, true},
        
        {"Paint", epd_bitmap_paint, ICON_PAINT, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("Paint App");
        }, true},
        
        {"Terminal", epd_bitmap_terminal, ICON_TERMINAL, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("Terminal");
        }, true},
        
        {"Script", epd_bitmap_script, ICON_SCRIPT, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("Script IDE");
        }, true},
        
        {"Setup", epd_bitmap_setup, ICON_SETUP, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("Settings");
        }, true},
        
        {"Retro", epd_bitmap_retro, ICON_RETRO, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(50, 100);
            gfx->print("Retro Games");
        }, true},
    };
    
    UIInterface::getInstance().drawIconGrid(appItems, 9, 4);
}

// Example 2: Top App Bar with Icon
void exampleAppBar() {
    UIInterface::getInstance().drawAppBar("Modbox System", epd_bitmap_setup);
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(20, 60);
    gfx->print("This is the main app bar");
    gfx->setCursor(20, 80);
    gfx->print("with icon integration");
}

// Example 3: Bottom Status Bar
void exampleStatusBar() {
    UIInterface::getInstance().drawStatusBar(epd_bitmap_wifi, epd_bitmap_ble, nullptr);
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(20, 100);
    gfx->print("Status Bar Example");
}

// Example 4: Icon Panels
void exampleIconPanels() {
    gfx->fillScreen(COLOR_BG);
    
    UIInterface::getInstance().drawIconPanel(10, 10, 100, 60, epd_bitmap_wifi, "WiFi Settings");
    UIInterface::getInstance().drawIconPanel(120, 10, 100, 60, epd_bitmap_ble, "BLE Connect");
    UIInterface::getInstance().drawIconPanel(10, 80, 100, 60, epd_bitmap_web, "Web Server");
    UIInterface::getInstance().drawIconPanel(120, 80, 100, 60, epd_bitmap_sd, "File Manager");
}

// Example 5: Icon Buttons
void exampleIconButtons() {
    gfx->fillScreen(COLOR_BG);
    
    UIInterface::getInstance().drawIconButton(20, 20, epd_bitmap_wifi, "WiFi");
    UIInterface::getInstance().drawIconButton(100, 20, epd_bitmap_ble, "BLE");
    UIInterface::getInstance().drawIconButton(180, 20, epd_bitmap_web, "Web");
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setTextSize(1);
    gfx->setCursor(20, 200);
    gfx->print("Icon Buttons - Press SELECT to activate");
}

// Example 6: Icon with Label (Simple Display)
void exampleSimpleIcons() {
    gfx->fillScreen(COLOR_BG);
    
    UIInterface::getInstance().drawIconWithLabel(epd_bitmap_wifi, "WiFi", 20, 20, COLOR_WHITE, COLOR_GREEN);
    UIInterface::getInstance().drawIconWithLabel(epd_bitmap_ble, "BLE", 100, 20, COLOR_WHITE, COLOR_BLUE);
    UIInterface::getInstance().drawIconWithLabel(epd_bitmap_web, "Web", 180, 20, COLOR_WHITE, COLOR_YELLOW);
    UIInterface::getInstance().drawIconWithLabel(epd_bitmap_sd, "SD Card", 20, 100, COLOR_WHITE, COLOR_WHITE);
    UIInterface::getInstance().drawIconWithLabel(epd_bitmap_paint, "Paint", 100, 100, COLOR_WHITE, COLOR_RED);
    UIInterface::getInstance().drawIconWithLabel(epd_bitmap_setup, "Setup", 180, 100, COLOR_WHITE, COLOR_GREEN);
}

// Example 7: Multi-level Menu with Icons
void exampleMultiLevelMenu() {
    // Main menu items
    MenuItem mainMenu[] = {
        {"System", epd_bitmap_setup, ICON_SETUP, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_GREEN);
            gfx->setCursor(10, 50);
            gfx->print("System Settings");
        }, true},
        
        {"Network", epd_bitmap_wifi, ICON_WIFI, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_GREEN);
            gfx->setCursor(10, 50);
            gfx->print("Network Settings");
        }, true},
        
        {"Storage", epd_bitmap_sd, ICON_SD, []() {
            gfx->fillScreen(COLOR_BG);
            gfx->setTextColor(COLOR_GREEN);
            gfx->setCursor(10, 50);
            gfx->print("Storage Settings");
        }, true},
    };
    
    UIInterface::getInstance().drawIconGrid(mainMenu, 3, 3);
}

#endif
