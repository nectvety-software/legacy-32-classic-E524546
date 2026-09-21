#include "BluetoothApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"

extern TFT_eSPI tft;

void BluetoothApp::begin() {
    state = STATE_MAIN;
    selectedMenu = 0;
    draw();
}

void BluetoothApp::update() {
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (input.isUp() && selectedMenu > 0) {
        selectedMenu--;
        draw();
    }
    else if (input.isDown() && selectedMenu < 2) {
        selectedMenu++;
        draw();
    }
    else if (input.isStart()) {
        if (selectedMenu == 0 && !bleServerRunning) {
            BLEDevice::init("NokiaOS");
            BLEServer* server = BLEDevice::createServer();
            bleServerRunning = true;
            state = STATE_CONNECTED;
            draw();
        }
    }
}

bool BluetoothApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void BluetoothApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("Bluetooth");
    
    const char* items[] = {"Start Server", "Scan Devices", "Connected"};
    for (int i = 0; i < 3; i++) {
        ui.drawMenuItem(i, items[i], i == selectedMenu, false);
    }
    
    ui.drawStatusBar();
    ui.drawText(bleServerRunning ? "Server Active" : "Server Off", 5, SCREEN_HEIGHT - STATUS_HEIGHT + 3, 
                bleServerRunning ? COLOR_SUCCESS : COLOR_TEXT_DIM);
}
