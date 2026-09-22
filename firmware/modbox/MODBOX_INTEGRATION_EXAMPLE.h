// ========================================
// MODBOX UI INTEGRATION EXAMPLE
// ========================================
// This file shows how to integrate the new UI Interface with existing modbox code

/*
// ADD TO modbox.cpp:

#include "ui_interface.h"
#include "ui_example.h"

// Global UI instance
UIInterface& ui = UIInterface::getInstance();

// ========================================
// UPDATED drawMainUI() function
// ========================================

void drawMainUI() {
    // Initialize UI interface
    ui.init();
    
    // Draw the main app grid with icons
    createAppGrid();
    
    // Draw status bar at bottom
    ui.drawStatusBar(epd_bitmap_wifi, epd_bitmap_ble, nullptr);
}

// ========================================
// UPDATED systemLoop() function
// ========================================

void systemLoop() {
    if (systemState == SYS_MAIN) {
        handleMainMenuNavigation();
    } else if (systemState == SYS_APP) {
        appLoop();
    }
}

// ========================================
// NEW FUNCTION: Menu Navigation Handler
// ========================================

void handleMainMenuNavigation() {
    // Handle UP button
    if (buttonPressed(KEY_UP)) {
        ui.navigateMenuUp();
        delay(150);
    }
    
    // Handle DOWN button
    if (buttonPressed(KEY_DOWN)) {
        ui.navigateMenuDown();
        delay(150);
    }
    
    // Handle LEFT button
    if (buttonPressed(KEY_LEFT)) {
        ui.navigateMenuLeft();
        delay(150);
    }
    
    // Handle RIGHT button
    if (buttonPressed(KEY_RIGHT)) {
        ui.navigateMenuRight();
        delay(150);
    }
    
    // Handle SELECT button (A button)
    if (buttonPressed(KEY_START)) {
        ui.activateSelectedItem();
        systemState = SYS_APP;
        delay(300);
    }
    
    // Handle MENU button to return
    if (buttonPressed(KEY_MENU)) {
        systemState = SYS_MAIN;
        drawMainUI();
        delay(300);
    }
}

// ========================================
// EXAMPLE: Update app creation
// ========================================

// Create app list with icons:
App appList[9] = {
    {"WiFi", epd_bitmap_wifi, appWiFi},
    {"BLE", epd_bitmap_ble, appBLE},
    {"Web", epd_bitmap_web, appWeb},
    {"SD Card", epd_bitmap_sd, appSD},
    {"Paint", epd_bitmap_paint, appPaint},
    {"Terminal", epd_bitmap_terminal, appCMD},
    {"Script", epd_bitmap_script, appScript},
    {"Setup", epd_bitmap_setup, appSetup},
    {"Retro", epd_bitmap_retro, appRetro},
};

// ========================================
// EXAMPLE: App Opening with transition
// ========================================

void openApp(uint8_t id) {
    if (id >= 9) return;
    
    // Clear screen
    gfx->fillScreen(COLOR_BG);
    
    // Draw app bar with icon
    ui.drawAppBar(appList[id].name, appList[id].icon);
    
    // Draw a loading animation
    for (int i = 0; i <= 100; i += 20) {
        gfx->fillRect(50, 150, (i * 2), 20, COLOR_GREEN);
        gfx->drawRect(50, 150, 200, 20, COLOR_WHITE);
        delay(50);
    }
    
    // Clear and run app
    gfx->fillScreen(COLOR_BG);
    cursorPos = id;
    systemState = SYS_APP;
    appList[id].run();
}

// ========================================
// EXAMPLE: Settings Menu with Icon Panels
// ========================================

void drawSettingsMenu() {
    gfx->fillScreen(COLOR_BG);
    
    // Draw title bar
    ui.drawAppBar("Settings", epd_bitmap_setup);
    
    // Draw settings panels
    ui.drawIconPanel(10, 50, 100, 70, epd_bitmap_wifi, "WiFi");
    ui.drawIconPanel(130, 50, 100, 70, epd_bitmap_ble, "Bluetooth");
    
    ui.drawIconPanel(10, 140, 100, 70, epd_bitmap_web, "Web Server");
    ui.drawIconPanel(130, 140, 100, 70, epd_bitmap_sd, "Storage");
    
    // Draw status bar
    ui.drawStatusBar(epd_bitmap_wifi, epd_bitmap_ble, nullptr);
}

// ========================================
// EXAMPLE: Info Panel Display
// ========================================

void drawWiFiInfo() {
    gfx->fillScreen(COLOR_BG);
    
    // Draw header with icon
    ui.drawAppBar("WiFi Status", epd_bitmap_wifi);
    
    // Draw info panel
    ui.drawPanel(10, 50, 220, 200, COLOR_BG, COLOR_GRAY);
    
    gfx->setTextColor(COLOR_GREEN);
    gfx->setTextSize(1);
    
    // WiFi icon + status
    ui.drawStatusIcon(epd_bitmap_wifi, 15, 60, COLOR_GREEN);
    gfx->setCursor(50, 60);
    gfx->print("Connected");
    
    // Network details
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(15, 90);
    gfx->print("SSID: MyNetwork");
    gfx->setCursor(15, 110);
    gfx->print("IP: 192.168.1.100");
    gfx->setCursor(15, 130);
    gfx->print("Signal: -55dBm");
    
    // Draw status bar
    ui.drawStatusBar(epd_bitmap_wifi, nullptr, nullptr);
}

// ========================================
// EXAMPLE: Multi-level Navigation Menu
// ========================================

void drawNetworkMenu() {
    gfx->fillScreen(COLOR_BG);
    
    MenuItem networkItems[] = {
        {"WiFi Config", epd_bitmap_wifi, ICON_WIFI, []() {
            drawWiFiInfo();
        }, true},
        
        {"Bluetooth", epd_bitmap_ble, ICON_BLE, []() {
            gfx->fillScreen(COLOR_BG);
            ui.drawAppBar("Bluetooth", epd_bitmap_ble);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(20, 100);
            gfx->print("BLE Menu");
        }, true},
        
        {"Web Server", epd_bitmap_web, ICON_WEB, []() {
            gfx->fillScreen(COLOR_BG);
            ui.drawAppBar("Web Server", epd_bitmap_web);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(20, 100);
            gfx->print("Web Server Menu");
        }, true},
    };
    
    ui.drawIconGrid(networkItems, 3, 3);
    ui.drawStatusBar(epd_bitmap_wifi, epd_bitmap_ble, nullptr);
}

// ========================================
// EXAMPLE: Custom Icon Display
// ========================================

void drawCustomIconPage() {
    gfx->fillScreen(COLOR_BG);
    
    // Draw title
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setTextSize(2);
    gfx->setCursor(20, 10);
    gfx->print("All Icons");
    
    // Display all icons in a grid
    ui.drawIconWithLabel(epd_bitmap_wifi, "WiFi", 20, 30);
    ui.drawIconWithLabel(epd_bitmap_ble, "BLE", 100, 30);
    ui.drawIconWithLabel(epd_bitmap_web, "Web", 180, 30);
    
    ui.drawIconWithLabel(epd_bitmap_sd, "SD", 20, 110);
    ui.drawIconWithLabel(epd_bitmap_paint, "Paint", 100, 110);
    ui.drawIconWithLabel(epd_bitmap_setup, "Setup", 180, 110);
    
    ui.drawIconWithLabel(epd_bitmap_script, "Script", 20, 190);
    ui.drawIconWithLabel(epd_bitmap_terminal, "Term", 100, 190);
    ui.drawIconWithLabel(epd_bitmap_retro, "Retro", 180, 190);
}

// ========================================
// USAGE IN MAIN LOOP
// ========================================

/*
void loop() {
    switch(systemState) {
        case SYS_BOOT:
            // Boot sequence already completed
            systemState = SYS_MAIN;
            break;
            
        case SYS_MAIN:
            handleMainMenuNavigation();
            break;
            
        case SYS_APP:
            appLoop();
            if (buttonPressed(KEY_MENU)) {
                systemState = SYS_MAIN;
                drawMainUI();
            }
            break;
            
        case SYS_POPUP:
            // Handle popup menu
            break;
    }
    
    delay(10);
}
*/

*/  // End of example code
