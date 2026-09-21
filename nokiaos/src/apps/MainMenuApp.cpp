#include "MainMenuApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"

extern TFT_eSPI tft;

void MainMenuApp::begin() {
    visibleItems = (SCREEN_HEIGHT - TITLE_HEIGHT - STATUS_HEIGHT) / MENU_ITEM_HEIGHT;
    selectedIndex = 0;
    topIndex = 0;
    draw();
}

void MainMenuApp::update() {
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (input.isUp() && selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < topIndex) topIndex = selectedIndex;
        draw();
    }
    else if (input.isDown() && selectedIndex < MENU_COUNT - 1) {
        selectedIndex++;
        if (selectedIndex >= topIndex + visibleItems) topIndex = selectedIndex - visibleItems + 1;
        draw();
    }
    else if (input.isStart()) {
        AppManager::getInstance().launchApp(menuApps[selectedIndex]);
    }
}

bool MainMenuApp::onKey(uint8_t key) {
    return true;
}

void MainMenuApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("NokiaOS");
    
    for (uint8_t i = 0; i < visibleItems && (topIndex + i) < MENU_COUNT; i++) {
        uint8_t idx = topIndex + i;
        bool isSelected = (idx == selectedIndex);
        ui.drawMenuItem(i, menuItems[idx], isSelected, true);
    }
    
    ui.drawStatusBar();
    ui.drawBattery(85, false);
    ui.drawSignalBars(4);
    
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", (millis() / 60000) % 24, (millis() / 1000) % 60);
    ui.drawTime(timeStr);
}
