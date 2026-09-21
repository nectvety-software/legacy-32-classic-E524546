#include "UIRenderer.h"
#include "SystemConfig.h"

UIRenderer UIRenderer::instance;

UIRenderer& UIRenderer::getInstance() {
    return instance;
}

void UIRenderer::begin(TFT_eSPI* tftPtr) {
    tft = tftPtr;
    tft->init();
    tft->setRotation(0);
    tft->fillScreen(COLOR_BG_DARK);
}

void UIRenderer::clearScreen(uint16_t color) {
    tft->fillScreen(color);
}

void UIRenderer::drawTitleBar(const char* title) {
    tft->fillRect(0, 0, SCREEN_WIDTH, TITLE_HEIGHT, COLOR_BG_MEDIUM);
    tft->setTextSize(1);
    tft->setTextFont(FONT_TITLE);
    tft->setTextColor(COLOR_TEXT);
    tft->setCursor(SCREEN_WIDTH / 2 - strlen(title) * 6, 8);
    tft->print(title);
}

void UIRenderer::drawStatusBar() {
    tft->fillRect(0, SCREEN_HEIGHT - STATUS_HEIGHT, SCREEN_WIDTH, STATUS_HEIGHT, COLOR_BG_MEDIUM);
}

void UIRenderer::drawBattery(uint8_t level, bool charging) {
    int bx = SCREEN_WIDTH - 30;
    int by = 5;
    tft->drawRect(bx, by, 22, 10, COLOR_TEXT);
    tft->fillRect(bx + 1, by + 1, 20 * level / 100, 8, 
                  level > 20 ? COLOR_SUCCESS : COLOR_WARNING);
    tft->fillRect(bx + 22, by + 3, 2, 4, COLOR_TEXT);
}

void UIRenderer::drawSignalBars(uint8_t level) {
    int sx = SCREEN_WIDTH - 50;
    int sy = 14;
    for (int i = 0; i < 4; i++) {
        int h = 3 + i * 2;
        uint16_t color = (i < level) ? COLOR_SUCCESS : COLOR_TEXT_DIM;
        tft->fillRect(sx + i * 4, sy - h, 3, h, color);
    }
}

void UIRenderer::drawTime(const char* time) {
    tft->setTextFont(FONT_STATUS);
    tft->setTextColor(COLOR_TEXT);
    tft->setCursor(5, SCREEN_HEIGHT - STATUS_HEIGHT + 5);
    tft->print(time);
}

void UIRenderer::drawMenuItem(int index, const char* text, bool selected, bool hasIcon) {
    int y = TITLE_HEIGHT + index * MENU_ITEM_HEIGHT;
    
    if (selected) {
        tft->fillRect(0, y, SCREEN_WIDTH, MENU_ITEM_HEIGHT, COLOR_SELECTED);
    }
    
    if (hasIcon) {
        tft->fillRect(5, y + 4, ICON_SIZE, ICON_SIZE, COLOR_ACCENT);
    }
    
    tft->setTextFont(FONT_MENU);
    tft->setTextColor(selected ? COLOR_BG_DARK : COLOR_TEXT);
    tft->setCursor(hasIcon ? 35 : 10, y + 10);
    tft->print(text);
}

void UIRenderer::drawListItem(int index, const char* title, const char* subtitle, bool selected) {
    int y = TITLE_HEIGHT + index * 40;
    
    if (selected) {
        tft->fillRect(0, y, SCREEN_WIDTH, 40, COLOR_SELECTED);
    }
    
    tft->setTextFont(FONT_MENU);
    tft->setTextColor(selected ? COLOR_BG_DARK : COLOR_TEXT);
    tft->setCursor(10, y + 5);
    tft->print(title);
    
    tft->setTextColor(selected ? COLOR_BG_MEDIUM : COLOR_TEXT_DIM);
    tft->setCursor(10, y + 22);
    tft->print(subtitle);
}

void UIRenderer::drawProgressBar(int x, int y, int width, int height, float progress) {
    tft->drawRect(x, y, width, height, COLOR_TEXT);
    tft->fillRect(x + 1, y + 1, (width - 2) * progress, height - 2, COLOR_PRIMARY);
}

void UIRenderer::drawButton(int x, int y, int width, int height, const char* text, bool selected) {
    uint16_t bgColor = selected ? COLOR_PRIMARY : COLOR_BG_MEDIUM;
    tft->fillRoundRect(x, y, width, height, 5, bgColor);
    tft->setTextColor(COLOR_TEXT);
    tft->setTextFont(FONT_MENU);
    tft->setCursor(x + width / 2 - strlen(text) * 3, y + height / 2 - 4);
    tft->print(text);
}

void UIRenderer::drawDialog(const char* title, const char* message) {
    int dlgWidth = SCREEN_WIDTH - 40;
    int dlgHeight = 100;
    int dlgX = 20;
    int dlgY = (SCREEN_HEIGHT - dlgHeight) / 2;
    
    tft->fillRoundRect(dlgX, dlgY, dlgWidth, dlgHeight, 10, COLOR_BG_LIGHT);
    tft->drawRoundRect(dlgX, dlgY, dlgWidth, dlgHeight, 10, COLOR_TEXT);
    
    tft->setTextFont(FONT_TITLE);
    tft->setTextColor(COLOR_ACCENT);
    tft->setCursor(dlgX + 10, dlgY + 10);
    tft->print(title);
    
    tft->setTextFont(FONT_BODY);
    tft->setTextColor(COLOR_TEXT);
    tft->setCursor(dlgX + 10, dlgY + 40);
    tft->print(message);
}

void UIRenderer::drawNotification(const char* message, uint16_t color) {
    int dlgWidth = SCREEN_WIDTH - 20;
    int dlgHeight = 35;
    int dlgX = 10;
    int dlgY = TITLE_HEIGHT + 10;
    
    tft->fillRoundRect(dlgX, dlgY, dlgWidth, dlgHeight, 5, color);
    tft->setTextColor(COLOR_TEXT);
    tft->setTextFont(FONT_MENU);
    tft->setCursor(dlgX + 10, dlgY + 12);
    tft->print(message);
}

void UIRenderer::fillRect(int x, int y, int w, int h, uint16_t color) {
    tft->fillRect(x, y, w, h, color);
}

void UIRenderer::drawRect(int x, int y, int w, int h, uint16_t color) {
    tft->drawRect(x, y, w, h, color);
}

void UIRenderer::drawText(const char* text, int x, int y, uint16_t color, uint8_t font) {
    tft->setTextFont(font);
    tft->setTextColor(color);
    tft->setCursor(x, y);
    tft->print(text);
}

void UIRenderer::drawTextCentered(const char* text, int y, uint16_t color, uint8_t font) {
    tft->setTextFont(font);
    tft->setTextColor(color);
    int16_t x = (SCREEN_WIDTH - strlen(text) * 6) / 2;
    tft->setCursor(x, y);
    tft->print(text);
}

int UIRenderer::getTextWidth(const char* text, uint8_t font) {
    return strlen(text) * 6;
}

int UIRenderer::getTextHeight(uint8_t font) {
    return 8;
}
