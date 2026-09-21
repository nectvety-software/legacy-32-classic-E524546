#include "LuaApp.h"
#include "../gui/UIRenderer.h"

void LuaApp::begin() { running = false; draw(); }
void LuaApp::update() {}

bool LuaApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void LuaApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("Lua Console");
    
    ui.drawTextCentered("Lua interpreter ready", 100, COLOR_TEXT_DIM);
    ui.drawTextCentered("Load scripts from SD", 130, COLOR_TEXT_DIM);
    
    ui.drawButton(60, 250, 120, 30, "Run Script", false);
    
    ui.drawStatusBar();
}
