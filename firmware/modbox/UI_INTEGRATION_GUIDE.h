// UI Interface Integration Guide for Modbox
// ========================================

/**
 * QUICK START GUIDE
 * 
 * 1. Include the UI Interface header in your main file:
 *    #include "ui_interface.h"
 *    #include "ui_example.h"
 * 
 * 2. Initialize the UI Interface in setup():
 *    UIInterface::getInstance().init();
 * 
 * 3. Use one of the example layouts or create your own
 */

// ========================================
// CORE CLASSES & OBJECTS
// ========================================

/**
 * UIInterface - Main interface management class (Singleton)
 * 
 * Usage:
 *   UIInterface& ui = UIInterface::getInstance();
 *   ui.drawIconGrid(items, count, cols);
 */

/**
 * MenuItem - Structure representing a menu item with icon
 * 
 * Fields:
 *   - label (const char*): Display text for item
 *   - icon (const uint16_t*): Pointer to icon bitmap
 *   - iconIndex (int): Index in icon manager
 *   - callback (void(*)()): Function to call when selected
 *   - enabled (bool): Whether item is active
 * 
 * Example:
 *   MenuItem item = {"WiFi", epd_bitmap_wifi, ICON_WIFI, []() { Serial.println("WiFi"); }, true};
 */

// ========================================
// CORE DRAWING FUNCTIONS
// ========================================

/**
 * drawIconGrid(MenuItem* items, int itemCount, int cols)
 * Draws a grid of icon buttons with labels
 * 
 * Parameters:
 *   items - Array of MenuItem structs
 *   itemCount - Number of items
 *   cols - Number of columns (default: 4)
 * 
 * Example:
 *   MenuItem apps[] = {
 *       {"WiFi", epd_bitmap_wifi, ICON_WIFI, wifiFunc, true},
 *       {"BLE", epd_bitmap_ble, ICON_BLE, bleFunc, true}
 *   };
 *   ui.drawIconGrid(apps, 2, 2);
 */

/**
 * drawIcon(const uint16_t* icon, int x, int y, int width, int height)
 * Draws a single icon bitmap at specified position
 * 
 * Parameters:
 *   icon - Icon bitmap pointer (typically epd_bitmap_xxx)
 *   x, y - Position on screen
 *   width, height - Icon dimensions (usually 48x48)
 * 
 * Example:
 *   ui.drawIcon(epd_bitmap_wifi, 50, 50, 48, 48);
 */

/**
 * drawIconWithLabel(const uint16_t* icon, const char* label, int x, int y)
 * Draws icon with text label below it
 * 
 * Parameters:
 *   icon - Icon bitmap pointer
 *   label - Text to display below icon
 *   x, y - Position on screen
 * 
 * Example:
 *   ui.drawIconWithLabel(epd_bitmap_wifi, "WiFi Settings", 20, 20);
 */

/**
 * drawAppBar(const char* title, const uint16_t* icon)
 * Draws app bar at top with optional icon
 * 
 * Parameters:
 *   title - Bar title text
 *   icon - Optional icon bitmap
 * 
 * Example:
 *   ui.drawAppBar("WiFi Setup", epd_bitmap_wifi);
 */

/**
 * drawStatusBar(const uint16_t* wifiIcon, const uint16_t* bleIcon, const uint16_t* batteryIcon)
 * Draws status bar at bottom with status icons
 * 
 * Parameters:
 *   wifiIcon - WiFi status icon (can be null)
 *   bleIcon - BLE status icon (can be null)
 *   batteryIcon - Battery status icon (can be null)
 * 
 * Example:
 *   ui.drawStatusBar(epd_bitmap_wifi, epd_bitmap_ble, nullptr);
 */

/**
 * drawIconPanel(int x, int y, int w, int h, const uint16_t* icon, const char* title)
 * Draws a panel with icon and title
 * 
 * Parameters:
 *   x, y - Panel position
 *   w, h - Panel dimensions
 *   icon - Icon bitmap
 *   title - Panel title text
 * 
 * Example:
 *   ui.drawIconPanel(10, 10, 100, 60, epd_bitmap_wifi, "WiFi Settings");
 */

/**
 * drawIconButton(int x, int y, const uint16_t* icon, const char* label, bool pressed)
 * Draws a clickable icon button
 * 
 * Parameters:
 *   x, y - Button position
 *   icon - Icon bitmap
 *   label - Button label
 *   pressed - Whether button appears pressed
 * 
 * Example:
 *   ui.drawIconButton(20, 20, epd_bitmap_wifi, "WiFi", false);
 */

/**
 * drawPanel(int x, int y, int w, int h, uint16_t bgColor, uint16_t borderColor)
 * Draws a rectangular panel with border
 * 
 * Example:
 *   ui.drawPanel(10, 10, 100, 50, COLOR_BG, COLOR_GRAY);
 */

/**
 * drawRoundedRect(int x, int y, int w, int h, int radius, uint16_t color)
 * Draws a rounded rectangle
 * 
 * Example:
 *   ui.drawRoundedRect(10, 10, 100, 50, 5, COLOR_WHITE);
 */

/**
 * drawSelectionFrame(int x, int y, int w, int h, uint16_t color, int thickness)
 * Draws a selection frame/highlight
 * 
 * Example:
 *   ui.drawSelectionFrame(20, 20, 60, 80, COLOR_YELLOW, 2);
 */

// ========================================
// NAVIGATION FUNCTIONS
// ========================================

/**
 * selectMenuItem(int index)
 * Selects a menu item by index
 * 
 * Example:
 *   ui.selectMenuItem(0);  // Select first item
 */

/**
 * navigateMenuUp()
 * Move selection up in grid
 */

/**
 * navigateMenuDown()
 * Move selection down in grid
 */

/**
 * navigateMenuLeft()
 * Move selection left in grid
 */

/**
 * navigateMenuRight()
 * Move selection right in grid
 */

/**
 * activateSelectedItem()
 * Triggers callback for selected item
 */

/**
 * getSelectedMenuItem()
 * Returns index of currently selected item
 */

// ========================================
// HELPER FUNCTIONS
// ========================================

/**
 * getGridX(int col)
 * Calculate X position for grid column
 */

/**
 * getGridY(int row)
 * Calculate Y position for grid row
 */

// ========================================
// USAGE EXAMPLES
// ========================================

// Example 1: Simple icon grid
void exampleSimpleGrid() {
    MenuItem items[] = {
        {"WiFi", epd_bitmap_wifi, ICON_WIFI, []() { Serial.println("WiFi"); }, true},
        {"BLE", epd_bitmap_ble, ICON_BLE, []() { Serial.println("BLE"); }, true},
    };
    
    UIInterface::getInstance().drawIconGrid(items, 2, 2);
}

// Example 2: App with status bar
void exampleWithStatusBar() {
    UIInterface& ui = UIInterface::getInstance();
    
    ui.drawAppBar("My App", epd_bitmap_setup);
    ui.drawStatusBar(epd_bitmap_wifi, epd_bitmap_ble, nullptr);
    
    // Draw content in middle
    gfx->fillRect(0, 40, 240, 260, COLOR_BG);
}

// Example 3: Navigation in menu
void exampleNavigation() {
    MenuItem items[] = {
        {"Item 1", epd_bitmap_wifi, ICON_WIFI, []() {}, true},
        {"Item 2", epd_bitmap_ble, ICON_BLE, []() {}, true},
        {"Item 3", epd_bitmap_web, ICON_WEB, []() {}, true},
        {"Item 4", epd_bitmap_sd, ICON_SD, []() {}, true},
    };
    
    UIInterface& ui = UIInterface::getInstance();
    ui.drawIconGrid(items, 4, 2);
    
    // Handle button presses
    // if (buttonUp) ui.navigateMenuUp();
    // if (buttonDown) ui.navigateMenuDown();
    // if (buttonSelect) ui.activateSelectedItem();
}

// Example 4: Custom icon panels
void exampleCustomPanels() {
    UIInterface& ui = UIInterface::getInstance();
    gfx->fillScreen(COLOR_BG);
    
    // Create a 2x2 grid of panels
    ui.drawIconPanel(10, 10, 110, 80, epd_bitmap_wifi, "WiFi");
    ui.drawIconPanel(130, 10, 110, 80, epd_bitmap_ble, "BLE");
    ui.drawIconPanel(10, 100, 110, 80, epd_bitmap_web, "Web");
    ui.drawIconPanel(130, 100, 110, 80, epd_bitmap_sd, "Files");
}

// ========================================
// INTEGRATION WITH MODBOX
// ========================================

// In modbox.cpp, update drawMainUI() function:
/*
void drawMainUI() {
    createAppGrid();  // From ui_example.h
    
    // In main loop, handle navigation:
    if (buttonPressed(KEY_UP)) {
        UIInterface::getInstance().navigateMenuUp();
        delay(200);
    }
    if (buttonPressed(KEY_DOWN)) {
        UIInterface::getInstance().navigateMenuDown();
        delay(200);
    }
    if (buttonPressed(KEY_START)) {
        UIInterface::getInstance().activateSelectedItem();
        delay(200);
    }
}
*/

// ========================================
// COLOR REFERENCE
// ========================================
// COLOR_BG     = 0x0000 (Black)
// COLOR_WHITE  = 0xFFFF (White)
// COLOR_YELLOW = 0xFD20 (Yellow)
// COLOR_GREEN  = 0x07E0 (Green)
// COLOR_RED    = 0xF800 (Red)
// COLOR_BLUE   = 0x041F (Blue)
// COLOR_GRAY   = 0x8410 (Gray)

// ========================================
// SIZE REFERENCE
// ========================================
// ICON_SIZE = 48x48 pixels
// SCREEN = 240x320 pixels
// GRID_COLS = 4 items per row (default)
// GRID_ROWS = 3 items per column (default)

