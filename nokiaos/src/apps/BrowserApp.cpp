#include "BrowserApp.h"
#include "../gui/UIRenderer.h"

void BrowserApp::begin() { selected = 0; draw(); }
void BrowserApp::update() {}

bool BrowserApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void BrowserApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("Web Browser");
    
    ui.drawTextCentered(WiFi.status() == WL_CONNECTED ? "WiFi Connected" : "WiFi Disconnected", 
                        80, WiFi.status() == WL_CONNECTED ? COLOR_SUCCESS : COLOR_ERROR);
    
    ui.fillRect(10, TITLE_HEIGHT + 50, SCREEN_WIDTH - 20, 30, COLOR_BG_LIGHT);
    ui.drawTextCentered(url, TITLE_HEIGHT + 58, COLOR_TEXT, 2);
    
    ui.drawButton(60, 250, 120, 30, "Go", false);
    
    ui.drawStatusBar();
}
