#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include "component/Display.h"
#include <cstring>
#include "component/Config.h"

// Keyboard layout constants
#define KEY_ROWS 5
#define KEY_COLS 10
#define KEY_WIDTH 28
#define KEY_HEIGHT 30
#define KEY_SPACING 2
#define KEYBOARD_HEIGHT 160

class VirtualKeyboard {
private:
    TFT_eSPI* tft;
    bool visible;
    char* inputBuffer;
    int maxLength;
    int cursorPos;
    
    // Keyboard layout
    static const char keyLayout[KEY_ROWS][KEY_COLS];
    
    void drawKey(int row, int col, char key);
    void drawCursor();
    void insertChar(char c);
    void deleteChar();

public:
    VirtualKeyboard(TFT_eSPI* display);
    void begin();
    void show(char* buffer, int maxLen);
    void hide();
    bool handleTouch(int x, int y);
};

#endif // VIRTUAL_KEYBOARD_H
