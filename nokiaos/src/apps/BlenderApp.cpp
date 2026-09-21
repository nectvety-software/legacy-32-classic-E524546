#include "BlenderApp.h"
#include "../gui/UIRenderer.h"

void BlenderApp::begin() { rotation = 0; selected = 0; draw(); }
void BlenderApp::update() { 
    rotation += 0.5; 
    if (rotation > 360) rotation = 0;
    draw();
}

bool BlenderApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void BlenderApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("3D Viewer");
    
    int cx = SCREEN_WIDTH / 2;
    int cy = (SCREEN_HEIGHT + TITLE_HEIGHT) / 2;
    int r = 50;
    
    uint16_t colors[] = {COLOR_FERROR, COLOR_SUCCESS, COLOR_PRIMARY, COLOR_ACCENT, COLOR_SECONDARY};
    
    for (int i = 0; i < 8; i++) {
        float angle = (rotation + i * 45) * 3.14159 / 180;
        int x = cx + r * cos(angle);
        int y = cy + r * sin(angle) * 0.6;
        ui.fillRect(x - 15, y - 15, 30, 30, colors[i % 5]);
    }
    
    ui.drawTextCentered("3D Cube Demo", 280, COLOR_TEXT_DIM);
    
    ui.drawStatusBar();
}
