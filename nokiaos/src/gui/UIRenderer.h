#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <TFT_eSPI.h>
#include "Theme.h"

class UIRenderer {
public:
    static UIRenderer& getInstance();
    
    void begin(TFT_eSPI* tft);
    TFT_eSPI* getTFT() { return tft; }
    
    void clearScreen(uint16_t color = COLOR_BG_DARK);
    void drawTitleBar(const char* title);
    void drawStatusBar();
    void drawBattery(uint8_t level, bool charging);
    void drawSignalBars(uint8_t level);
    void drawTime(const char* time);
    
    void drawMenuItem(int index, const char* text, bool selected, bool hasIcon = false);
    void drawIcon(int x, int y, const char* iconName);
    
    void drawListItem(int index, const char* title, const char* subtitle, bool selected);
    
    void drawProgressBar(int x, int y, int width, int height, float progress);
    void drawButton(int x, int y, int width, int height, const char* text, bool selected);
    
    void drawDialog(const char* title, const char* message);
    void drawNotification(const char* message, uint16_t color = COLOR_ACCENT);
    
    void drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data);
    
    void drawText(const char* text, int x, int y, uint16_t color = COLOR_TEXT, uint8_t font = FONT_BODY);
    void drawTextCentered(const char* text, int y, uint16_t color = COLOR_TEXT, uint8_t font = FONT_BODY);
    
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    
    int getTextWidth(const char* text, uint8_t font = FONT_BODY);
    int getTextHeight(uint8_t font = FONT_BODY);

private:
    UIRenderer() : tft(nullptr), cursorX(0), cursorY(0) {}
    
    static UIRenderer instance;
    TFT_eSPI* tft;
    int cursorX;
    int cursorY;
    
    void drawPixel(int x, int y, uint16_t color);
};

#endif
