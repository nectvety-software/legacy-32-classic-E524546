#include "MediaApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"

extern TFT_eSPI tft;

void MediaApp::begin() {
    isPlaying = false;
    selectedFile = 0;
    root = SD.open("/");
    draw();
}

void MediaApp::end() {
    root.close();
}

void MediaApp::update() {
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (input.isUp() && selectedFile > 0) {
        selectedFile--;
        draw();
    }
    else if (input.isDown()) {
        selectedFile++;
        draw();
    }
    else if (input.isStart()) {
        isPlaying = !isPlaying;
        draw();
    }
}

bool MediaApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void MediaApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("Media Player");
    
    ui.drawTextCentered(isPlaying ? "Playing..." : "Stopped", SCREEN_HEIGHT / 2 - 20, 
                        isPlaying ? COLOR_SUCCESS : COLOR_TEXT_DIM);
    
    ui.drawButton(SCREEN_WIDTH / 2 - 40, SCREEN_HEIGHT / 2 + 20, 80, 30, 
                  isPlaying ? "Pause" : "Play", true);
    
    ui.drawStatusBar();
    ui.drawText(selectedFile == 0 ? "Track 1" : "Track 2", 5, SCREEN_HEIGHT - STATUS_HEIGHT + 3, COLOR_TEXT_DIM);
}
