#include "SettingsApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"

extern TFT_eSPI tft;

void SettingsApp::begin() {
    selectedItem = 0;
    brightness = 128;
    draw();
}

void SettingsApp::update() {
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (input.isUp() && selectedItem > 0) {
        selectedItem--;
        draw();
    }
    else if (input.isDown() && selectedItem < 3) {
        selectedItem++;
        draw();
    }
    else if (input.isLeft() || input.isRight()) {
        if (selectedItem == 1) {
            brightness = (brightness + (input.isRight() ? 32 : -32));
            if (brightness > 255) brightness = 255;
            if (brightness < 32) brightness = 32;
            ledcWrite(0, brightness);
        }
        draw();
    }
    else if (input.isStart()) {
        if (selectedItem == 0) {
            tft.invertDisplay(true);
        } else if (selectedItem == 3) {
            ESP.restart();
        }
    }
}

bool SettingsApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void SettingsApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("Settings");
    
    const char* items[] = {"Invert Display", "Brightness", "Sound", "Reboot"};
    for (int i = 0; i < 4; i++) {
        ui.drawMenuItem(i, items[i], i == selectedItem, false);
    }
    
    if (selectedItem == 1) {
        ui.drawProgressBar(10, TITLE_HEIGHT + 1 * MENU_ITEM_HEIGHT + 5, SCREEN_WIDTH - 20, 10, (float)brightness / 255.0);
    }
    
    ui.drawStatusBar();
}
