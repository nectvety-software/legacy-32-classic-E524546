#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include "modbox_main.h"
#include "icon_manager.h"

////////////// UI CONFIG //////////////
#define UI_ITEM_WIDTH 60
#define UI_ITEM_HEIGHT 80
#define UI_ICON_SIZE 48
#define UI_GRID_COLS 4
#define UI_GRID_ROWS 3
#define UI_PADDING 10
#define UI_LABEL_HEIGHT 20

////////////// MENU ITEM STRUCT //////////////
struct MenuItem {
    const char* label;
    const uint16_t* icon;
    int iconIndex;
    void (*callback)();
    bool enabled;
};

////////////// UI INTERFACE CLASS //////////////
class UIInterface {
public:
    static UIInterface& getInstance() {
        static UIInterface instance;
        return instance;
    }
    
    // Initialization
    void init();
    
    // Grid Layout Methods
    void drawIconGrid(MenuItem* items, int itemCount, int cols = UI_GRID_COLS);
    void drawIconMenuItem(MenuItem* item, int x, int y, bool selected = false);
    void drawIconWithLabel(const uint16_t* icon, const char* label, int x, int y, uint16_t iconColor = COLOR_WHITE, uint16_t labelColor = COLOR_WHITE);
    void drawIcon(const uint16_t* icon, int x, int y, int width = UI_ICON_SIZE, int height = UI_ICON_SIZE);
    
    // Menu Navigation
    void selectMenuItem(int index);
    int getSelectedMenuItem() { return selectedIndex; }
    void activateSelectedItem();
    void navigateMenuUp();
    void navigateMenuDown();
    void navigateMenuLeft();
    void navigateMenuRight();
    
    // Icon Drawing Utilities
    void drawRoundedRect(int x, int y, int w, int h, int radius, uint16_t color);
    void drawSelectionFrame(int x, int y, int w, int h, uint16_t color, int thickness = 2);
    void drawIconButton(int x, int y, const uint16_t* icon, const char* label, bool pressed = false);
    void drawStatusIcon(const uint16_t* icon, int x, int y, uint16_t color = COLOR_GREEN);
    
    // Panel Drawing
    void drawPanel(int x, int y, int w, int h, uint16_t bgColor = COLOR_BG, uint16_t borderColor = COLOR_GRAY);
    void drawIconPanel(int x, int y, int w, int h, const uint16_t* icon, const char* title);
    void drawAppBar(const char* title, const uint16_t* icon = nullptr);
    
    // Status Bar with Icons
    void drawStatusBar(const uint16_t* wifiIcon = nullptr, const uint16_t* bleIcon = nullptr, 
                      const uint16_t* batteryIcon = nullptr);
    
    // Layout Helpers
    int getGridX(int col) { return UI_PADDING + (col * (UI_ITEM_WIDTH + UI_PADDING)); }
    int getGridY(int row) { return UI_PADDING + (row * (UI_ITEM_HEIGHT + UI_PADDING)); }
    
private:
    int selectedIndex = 0;
    int grid_cols = UI_GRID_COLS;
    MenuItem* currentItems = nullptr;
    int itemCount = 0;
    
    UIInterface() {}
};

#endif
