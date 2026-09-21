#include "virtual_keyboard.h"

VirtualKeyboard::VirtualKeyboard(TFT_eSPI* display) {
    tft = display;
    visible = false;
    inputBuffer = nullptr;
    maxLength = 0;
    cursorPos = 0;
}

void VirtualKeyboard::begin() {
    // Initialize keyboard settings
}

void VirtualKeyboard::show(char* buffer, int maxLen) {
    inputBuffer = buffer;
    maxLength = maxLen;
    cursorPos = strlen(buffer);
    visible = true;
    
    int keyboardY = 320 - KEYBOARD_HEIGHT;
    
    // Draw keyboard background
    tft->fillRect(0, keyboardY, 240, KEYBOARD_HEIGHT, 0x3186); // Dark grey, RGB565
    
    // Draw all keys
    for (int row = 0; row < KEY_ROWS; row++) {
        for (int col = 0; col < KEY_COLS; col++) {
            drawKey(row, col, keyLayout[row][col]);
        }
    }
    
    // Draw input field above keyboard
    tft->fillRect(5, keyboardY - 35, 230, 30, BG_COLOR); // Background
    tft->drawRect(5, keyboardY - 35, 230, 30, THEME_COLOR); // Theme color
    tft->setTextColor(TEXT_COLOR, BG_COLOR);
    tft->setCursor(10, keyboardY - 25);
    tft->print(inputBuffer);
    
    drawCursor();
}

void VirtualKeyboard::hide() {
    visible = false;
    // Clear keyboard area
    tft->fillRect(0, 320 - KEYBOARD_HEIGHT, 240, KEYBOARD_HEIGHT, BG_COLOR);
}

void VirtualKeyboard::drawKey(int row, int col, char key) {
    int x = 3 + col * (KEY_WIDTH + KEY_SPACING);
    int y = 320 - KEYBOARD_HEIGHT + 5 + row * (KEY_HEIGHT + KEY_SPACING);
    
    // Special handling for space bar
    if (row == 4 && col < 7) {
        if (col == 0) {
            x = 5;
            int spaceWidth = 7 * (KEY_WIDTH + KEY_SPACING) - KEY_SPACING;
            tft->drawRect(x, y, spaceWidth, KEY_HEIGHT, 0xFFFF);
            tft->setTextColor(0xFFFF, 0x4208);
            tft->setCursor(x + spaceWidth/2 - 20, y + 8);
            tft->print("SPACE");
        }
        return;
    }
    
    // Draw key border
    tft->drawRect(x, y, KEY_WIDTH, KEY_HEIGHT, HL_COLOR);
    
    // Draw key label
    tft->setTextColor(TEXT_COLOR, 0x3186);
    tft->setCursor(x + 8, y + 8);
    
    switch (key) {
        case '\b':
            tft->print("BKSP");
            break;
        case '\177':
            tft->print("CLR");
            break;
        case '\n':
            tft->print("GO");
            break;
        case '\r':
            tft->print("HIDE");
            break;
        default:
            tft->print(key);
            break;
    }
}

void VirtualKeyboard::drawCursor() {
    if (!inputBuffer || !visible) return;
    
    int keyboardY = 240 - KEYBOARD_HEIGHT;
    int cursorX = 10 + tft->textWidth(inputBuffer);
    
    tft->drawLine(cursorX, keyboardY - 25, cursorX, keyboardY - 15, THEME_COLOR);
}

void VirtualKeyboard::insertChar(char c) {
    if (!inputBuffer || strlen(inputBuffer) >= maxLength - 1) return;
    
    int len = strlen(inputBuffer);
    
    // Shift characters right if cursor is not at end
    for (int i = len; i > cursorPos; i--) {
        inputBuffer[i] = inputBuffer[i - 1];
    }
    
    inputBuffer[cursorPos] = c;
    inputBuffer[cursorPos + 1] = '\0';
    cursorPos++;
}

void VirtualKeyboard::deleteChar() {
    if (!inputBuffer || cursorPos == 0) return;
    
    int len = strlen(inputBuffer);
    
    // Shift characters left
    for (int i = cursorPos - 1; i < len; i++) {
        inputBuffer[i] = inputBuffer[i + 1];
    }
    
    cursorPos--;
}

bool VirtualKeyboard::handleTouch(int x, int y) {
    if (!visible) return false;
    
    int keyboardY = 240 - KEYBOARD_HEIGHT;
    
    // Check if touch is in keyboard area
    if (y < keyboardY) return false;
    
    // Check which key was touched
    for (int row = 0; row < KEY_ROWS; row++) {
        for (int col = 0; col < KEY_COLS; col++) {
            int keyX = 5 + col * (KEY_WIDTH + KEY_SPACING);
            int keyY = keyboardY + 5 + row * (KEY_HEIGHT + KEY_SPACING);
            int keyW = KEY_WIDTH;
            int keyH = KEY_HEIGHT;
            
            // Special case for space bar
            if (row == 4 && col < 7) {
                if (col == 0) {
                    keyX = 5;
                    keyW = 7 * (KEY_WIDTH + KEY_SPACING) - KEY_SPACING;
                } else {
                    continue; // Skip other keys in space bar row
                }
            }
            
            if (x >= keyX && x <= keyX + keyW && y >= keyY && y <= keyY + keyH) {
                char key = keyLayout[row][col];
                
                switch (key) {
                    case '\b':
                        deleteChar();
                        break;
                    case '\177':
                        strcpy(inputBuffer, "");
                        cursorPos = 0;
                        break;
                    case '\n':
                        return true; // Signal URL navigation
                    case '\r':
                        hide();
                        return false;
                    default:
                        insertChar(key);
                        break;
                }
                
                // Redraw input field and cursor
                int inputY = keyboardY - 35;
                tft->fillRect(5, inputY, 310, 30, BG_COLOR);
                tft->drawRect(5, inputY, 310, 30, THEME_COLOR);
                tft->setTextColor(TEXT_COLOR, BG_COLOR);
                tft->setCursor(10, inputY + 10);
                tft->print(inputBuffer);
                drawCursor();
                
                return false;
            }
        }
    }
    
    return false;
}
