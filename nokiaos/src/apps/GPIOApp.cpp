#include "GPIOApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"

extern TFT_eSPI tft;

const uint8_t GPIOApp::GPIO_PINS[8] = {1, 2, 3, 4, 10, 11, 12, 13};
const char* GPIOApp::GPIO_NAMES[8] = {"GPIO1", "GPIO2", "GPIO3", "GPIO4", "GPIO10", "GPIO11", "GPIO12", "GPIO13"};

void GPIOApp::begin() {
    selectedPin = 0;
    for (int i = 0; i < 8; i++) {
        pinMode(GPIO_PINS[i], OUTPUT);
        digitalWrite(GPIO_PINS[i], LOW);
        pinModes[i] = OUTPUT;
        pinStates[i] = false;
    }
    draw();
}

void GPIOApp::update() {
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (input.isUp() && selectedPin > 0) {
        selectedPin--;
        draw();
    }
    else if (input.isDown() && selectedPin < 7) {
        selectedPin++;
        draw();
    }
    else if (input.isLeft()) {
        pinModes[selectedPin] = (pinModes[selectedPin] == OUTPUT) ? INPUT : OUTPUT;
        pinMode(GPIO_PINS[selectedPin], pinModes[selectedPin]);
        draw();
    }
    else if (input.isRight()) {
        if (pinModes[selectedPin] == OUTPUT) {
            pinStates[selectedPin] = !pinStates[selectedPin];
            digitalWrite(GPIO_PINS[selectedPin], pinStates[selectedPin] ? HIGH : LOW);
            draw();
        }
    }
}

bool GPIOApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void GPIOApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("GPIO Control");
    
    for (int i = 0; i < 6; i++) {
        int y = TITLE_HEIGHT + 10 + i * 45;
        bool isSelected = (i == selectedPin);
        
        if (isSelected) {
            ui.fillRect(0, y, SCREEN_WIDTH, 40, COLOR_SELECTED);
        }
        
        ui.drawText(GPIO_NAMES[i], 10, y + 5, isSelected ? COLOR_BG_DARK : COLOR_TEXT);
        
        const char* modeStr = pinModes[i] == OUTPUT ? "OUT" : "IN";
        uint16_t color = pinModes[i] == OUTPUT ? COLOR_PRIMARY : COLOR_TEXT_DIM;
        ui.drawText(modeStr, 10, y + 22, isSelected ? COLOR_BG_MEDIUM : color);
        
        if (pinModes[i] == OUTPUT) {
            uint16_t stateColor = pinStates[i] ? COLOR_SUCCESS : COLOR_ERROR;
            const char* stateStr = pinStates[i] ? "HIGH" : "LOW";
            ui.drawText(stateStr, 70, y + 22, isSelected ? stateColor : stateColor);
        }
    }
    
    ui.drawStatusBar();
    ui.drawTextCentered("LEFT: Mode | RIGHT: Toggle", SCREEN_HEIGHT - STATUS_HEIGHT + 3, COLOR_TEXT_DIM);
}
