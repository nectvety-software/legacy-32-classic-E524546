#include "NotesApp.h"
#include "../gui/UIRenderer.h"

void NotesApp::begin() { cursorPos = 0; draw(); }
void NotesApp::update() {}

bool NotesApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void NotesApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("Notes");
    
    ui.fillRect(5, TITLE_HEIGHT + 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - TITLE_HEIGHT - STATUS_HEIGHT - 15, COLOR_BG_LIGHT);
    ui.drawText(notes, 10, TITLE_HEIGHT + 10, COLOR_TEXT, 2);
    
    ui.drawStatusBar();
}
