#include "AboutApp.h"
#include "../gui/UIRenderer.h"

void AboutApp::begin() { draw(); }
void AboutApp::update() {}

bool AboutApp::onKey(uint8_t key) {
    if (key == KEY_MENU || key == KEY_B) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void AboutApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("About");
    
    ui.drawTextCentered("NokiaOS", 60, COLOR_ACCENT, 4);
    ui.drawTextCentered("v1.0.0", 90, COLOR_TEXT_DIM);
    ui.drawTextCentered("ESP32-S3 Edition", 120, COLOR_TEXT);
    
    ui.drawTextCentered("Build: Apr 2026", 180, COLOR_TEXT_DIM);
    ui.drawTextCentered("Powered by TFT_eSPI", 200, COLOR_TEXT_DIM);
    
    ui.drawStatusBar();
}
