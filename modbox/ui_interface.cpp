#include "ui_interface.h"

void UIInterface::init() {
    // Initialize UI
    selectedIndex = 0;
    Serial.println("UI Interface initialized");
}

void UIInterface::drawIcon(const uint16_t* icon, int x, int y, int width, int height) {
    if (!icon) return;
    
    // Draw icon as 16-bit color bitmap
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            uint16_t color = icon[row * width + col];
            if (color != 0x0000) {  // Skip transparent (black)
                gfx->drawPixel(x + col, y + row, color);
            }
        }
    }
}

void UIInterface::drawIconWithLabel(const uint16_t* icon, const char* label, int x, int y, 
                                     uint16_t iconColor, uint16_t labelColor) {
    // Draw icon
    if (icon) {
        drawIcon(icon, x, y, UI_ICON_SIZE, UI_ICON_SIZE);
    }
    
    // Draw label below icon
    gfx->setTextColor(labelColor);
    gfx->setTextSize(1);
    gfx->setCursor(x, y + UI_ICON_SIZE + 5);
    gfx->print(label);
}

void UIInterface::drawRoundedRect(int x, int y, int w, int h, int radius, uint16_t color) {
    // Draw rounded rectangle
    gfx->drawLine(x + radius, y, x + w - radius, y, color);                          // Top
    gfx->drawLine(x + w - 1, y + radius, x + w - 1, y + h - radius, color);          // Right
    gfx->drawLine(x + w - radius, y + h - 1, x + radius, y + h - 1, color);          // Bottom
    gfx->drawLine(x, y + radius, x, y + h - radius, color);                          // Left
    
    // Draw corners
    for (int i = 0; i < radius; i++) {
        gfx->drawPixel(x + radius - i - 1, y + radius - i - 1, color);               // Top-left
        gfx->drawPixel(x + w - radius + i, y + radius - i - 1, color);               // Top-right
        gfx->drawPixel(x + w - radius + i, y + h - radius + i - 1, color);           // Bottom-right
        gfx->drawPixel(x + radius - i - 1, y + h - radius + i - 1, color);           // Bottom-left
    }
}

void UIInterface::drawSelectionFrame(int x, int y, int w, int h, uint16_t color, int thickness) {
    for (int i = 0; i < thickness; i++) {
        gfx->drawRect(x - i, y - i, w + (i * 2), h + (i * 2), color);
    }
}

void UIInterface::drawIconButton(int x, int y, const uint16_t* icon, const char* label, bool pressed) {
    uint16_t bgColor = pressed ? COLOR_GRAY : COLOR_BG;
    uint16_t borderColor = pressed ? COLOR_YELLOW : COLOR_WHITE;
    
    // Draw button background
    gfx->fillRect(x, y, UI_ITEM_WIDTH, UI_ITEM_HEIGHT, bgColor);
    
    // Draw border
    gfx->drawRect(x, y, UI_ITEM_WIDTH, UI_ITEM_HEIGHT, borderColor);
    
    // Draw icon (centered)
    int iconX = x + (UI_ITEM_WIDTH - UI_ICON_SIZE) / 2;
    int iconY = y + 10;
    if (icon) {
        drawIcon(icon, iconX, iconY, UI_ICON_SIZE, UI_ICON_SIZE);
    }
    
    // Draw label
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    int labelX = x + 5;
    int labelY = y + UI_ICON_SIZE + 15;
    gfx->setCursor(labelX, labelY);
    gfx->print(label);
}

void UIInterface::drawStatusIcon(const uint16_t* icon, int x, int y, uint16_t color) {
    // Draw small icon with status color
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 24; col++) {
            uint16_t pixelColor = icon[row * 24 + col];
            if (pixelColor != 0x0000) {
                gfx->drawPixel(x + col, y + row, color);
            }
        }
    }
}

void UIInterface::drawPanel(int x, int y, int w, int h, uint16_t bgColor, uint16_t borderColor) {
    // Draw panel background
    gfx->fillRect(x, y, w, h, bgColor);
    
    // Draw panel border
    gfx->drawRect(x, y, w, h, borderColor);
}

void UIInterface::drawIconPanel(int x, int y, int w, int h, const uint16_t* icon, const char* title) {
    // Draw panel
    drawPanel(x, y, w, h, COLOR_BG, COLOR_GRAY);
    
    // Draw icon
    int iconX = x + 10;
    int iconY = y + 10;
    if (icon) {
        drawIcon(icon, iconX, iconY, 32, 32);
    }
    
    // Draw title
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(iconX + 40, iconY + 10);
    gfx->print(title);
}

void UIInterface::drawAppBar(const char* title, const uint16_t* icon) {
    uint16_t barHeight = 40;
    
    // Draw background
    gfx->fillRect(0, 0, SCREEN_WIDTH, barHeight, COLOR_BLUE);
    
    // Draw icon (if provided)
    if (icon) {
        drawIcon(icon, 5, 5, 32, 32);
    }
    
    // Draw title
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(icon ? 45 : 15, 10);
    gfx->print(title);
}

void UIInterface::drawStatusBar(const uint16_t* wifiIcon, const uint16_t* bleIcon, 
                                const uint16_t* batteryIcon) {
    uint16_t barHeight = 25;
    uint16_t statusY = SCREEN_HEIGHT - barHeight;
    
    // Draw background
    gfx->fillRect(0, statusY, SCREEN_WIDTH, barHeight, COLOR_GRAY);
    gfx->drawLine(0, statusY, SCREEN_WIDTH, statusY, COLOR_WHITE);
    
    // Draw WiFi icon
    if (wifiIcon) {
        drawStatusIcon(wifiIcon, 5, statusY + 2, COLOR_GREEN);
    }
    
    // Draw BLE icon
    if (bleIcon) {
        drawStatusIcon(bleIcon, 35, statusY + 2, COLOR_BLUE);
    }
    
    // Draw Battery icon
    if (batteryIcon) {
        drawStatusIcon(batteryIcon, SCREEN_WIDTH - 30, statusY + 2, COLOR_GREEN);
    }
    
    // Draw time/status text
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(65, statusY + 5);
    gfx->print("Ready");
}

void UIInterface::drawIconGrid(MenuItem* items, int itemCount, int cols) {
    if (!items || itemCount <= 0) return;
    
    currentItems = items;
    this->itemCount = itemCount;
    grid_cols = cols;
    
    gfx->fillScreen(COLOR_BG);
    
    // Draw each item in grid
    for (int i = 0; i < itemCount; i++) {
        int row = i / cols;
        int col = i % cols;
        
        int x = getGridX(col);
        int y = getGridY(row);
        
        drawIconMenuItem(&items[i], x, y, i == selectedIndex);
    }
}

void UIInterface::drawIconMenuItem(MenuItem* item, int x, int y, bool selected) {
    if (!item) return;
    
    uint16_t bgColor = selected ? COLOR_BLUE : COLOR_BG;
    uint16_t borderColor = selected ? COLOR_YELLOW : COLOR_GRAY;
    
    // Draw button background
    drawPanel(x, y, UI_ITEM_WIDTH, UI_ITEM_HEIGHT, bgColor, borderColor);
    
    // Draw icon
    if (item->icon) {
        int iconX = x + (UI_ITEM_WIDTH - UI_ICON_SIZE) / 2;
        int iconY = y + 5;
        drawIcon(item->icon, iconX, iconY, UI_ICON_SIZE, UI_ICON_SIZE);
    }
    
    // Draw label
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    int labelX = x + 5;
    int labelY = y + UI_ICON_SIZE + 10;
    gfx->setCursor(labelX, labelY);
    gfx->print(item->label);
}

void UIInterface::selectMenuItem(int index) {
    if (index >= 0 && index < itemCount) {
        selectedIndex = index;
        
        // Redraw previously selected and current selected items
        int row = index / grid_cols;
        int col = index % grid_cols;
        
        int x = getGridX(col);
        int y = getGridY(row);
        
        drawIconMenuItem(&currentItems[index], x, y, true);
    }
}

void UIInterface::navigateMenuUp() {
    int newIndex = selectedIndex - grid_cols;
    if (newIndex >= 0) {
        int prevCol = selectedIndex % grid_cols;
        int newCol = newIndex % grid_cols;
        
        // Redraw previous item
        int prevRow = selectedIndex / grid_cols;
        drawIconMenuItem(&currentItems[selectedIndex], 
                        getGridX(prevCol), getGridY(prevRow), false);
        
        selectMenuItem(newIndex);
    }
}

void UIInterface::navigateMenuDown() {
    int newIndex = selectedIndex + grid_cols;
    if (newIndex < itemCount) {
        int prevCol = selectedIndex % grid_cols;
        int newCol = newIndex % grid_cols;
        
        // Redraw previous item
        int prevRow = selectedIndex / grid_cols;
        drawIconMenuItem(&currentItems[selectedIndex], 
                        getGridX(prevCol), getGridY(prevRow), false);
        
        selectMenuItem(newIndex);
    }
}

void UIInterface::navigateMenuLeft() {
    int newIndex = selectedIndex - 1;
    if ((newIndex % grid_cols) != (grid_cols - 1) && newIndex >= 0) {
        int prevCol = selectedIndex % grid_cols;
        
        // Redraw previous item
        int prevRow = selectedIndex / grid_cols;
        drawIconMenuItem(&currentItems[selectedIndex], 
                        getGridX(prevCol), getGridY(prevRow), false);
        
        selectMenuItem(newIndex);
    }
}

void UIInterface::navigateMenuRight() {
    int newIndex = selectedIndex + 1;
    if ((newIndex % grid_cols) != 0 && newIndex < itemCount) {
        int prevCol = selectedIndex % grid_cols;
        
        // Redraw previous item
        int prevRow = selectedIndex / grid_cols;
        drawIconMenuItem(&currentItems[selectedIndex], 
                        getGridX(prevCol), getGridY(prevRow), false);
        
        selectMenuItem(newIndex);
    }
}

void UIInterface::activateSelectedItem() {
    if (selectedIndex >= 0 && selectedIndex < itemCount) {
        if (currentItems[selectedIndex].enabled && currentItems[selectedIndex].callback) {
            currentItems[selectedIndex].callback();
        }
    }
}
