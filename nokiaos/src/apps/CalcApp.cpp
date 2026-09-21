#include "CalcApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"
#include <stdio.h>

extern TFT_eSPI tft;

void CalcApp::begin() {
    strcpy(display, "0");
    operand1 = operand2 = 0;
    operation = 0;
    startNewNumber = true;
    selectedBtn = 0;
    draw();
}

void CalcApp::update() {
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (input.isUp() && selectedBtn >= 3) selectedBtn -= 4;
    else if (input.isDown() && selectedBtn < 12) selectedBtn += 4;
    else if (input.isLeft() && selectedBtn > 0) selectedBtn--;
    else if (input.isRight() && selectedBtn < 15) selectedBtn++;
    else if (input.isStart()) {
        handleButton(selectedBtn);
    }
    draw();
}

void CalcApp::handleButton(int btn) {
    if (btn >= 0 && btn <= 9) {
        appendDigit(btn);
    } else if (btn == 10) {
        if (operation) calculate();
        operation = '+';
        startNewNumber = true;
    } else if (btn == 11) {
        if (operation) calculate();
        operation = '-';
        startNewNumber = true;
    } else if (btn == 12) {
        calculate();
    } else if (btn == 14) {
        strcpy(display, "0");
        operand1 = operand2 = 0;
        operation = 0;
        startNewNumber = true;
    }
}

void CalcApp::appendDigit(int d) {
    if (startNewNumber) {
        strcpy(display, "");
        startNewNumber = false;
    }
    if (strlen(display) < 10) {
        char tmp[2] = "0";
        tmp[0] = '0' + d;
        strcat(display, tmp);
    }
}

void CalcApp::calculate() {
    operand2 = atof(display);
    switch (operation) {
        case '+': operand1 = operand1 + operand2; break;
        case '-': operand1 = operand1 - operand2; break;
        case '*': operand1 = operand1 * operand2; break;
        case '/': if (operand2 != 0) operand1 = operand1 / operand2; break;
    }
    snprintf(display, sizeof(display), "%.8g", operand1);
    operation = 0;
    startNewNumber = true;
}

bool CalcApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void CalcApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar("Calculator");
    
    tft.fillRoundRect(10, TITLE_HEIGHT + 5, SCREEN_WIDTH - 20, 40, 5, COLOR_BG_LIGHT);
    ui.drawTextCentered(display, TITLE_HEIGHT + 18, COLOR_TEXT);
    
    const char* buttons[] = {"7", "8", "9", "+", "4", "5", "6", "-", "1", "2", "3", "*", "C", "0", "=", "/"};
    for (int i = 0; i < 16; i++) {
        int x = 10 + (i % 4) * 57;
        int y = TITLE_HEIGHT + 55 + (i / 4) * 50;
        bool selected = (i == selectedBtn);
        ui.drawButton(x, y, 50, 40, buttons[i], selected);
    }
}
