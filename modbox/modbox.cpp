#include "modbox_main.h"
#include "icon_manager.h"
#include "assets/icons_bitmap.h"
#include "icon_test.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0, true, 240, 320);

SystemState systemState = SYS_BOOT;
int cursorPos = 0;
int lastButtonState = HIGH;
String deviceUsername = "Admin";

////////////// SHARED KEYBOARD //////////////

const char* keyboardRowsUpper = 
    "1234567890\b"
    "QWERTYUIOP"
    "ASDFGHJKL"
    "\tZXCVBNM,."
    " a<>";

const char* keyboardRowsLower = 
    "1234567890\b"
    "qwertyuiop"
    "asdfghjkl"
    "\tzxcvbnm,."
    " a<>";

int kbdCursor = 0;
int lastKbdCursor = -1;
bool isUppercase = true;

const int rowStartX[5] = {10, 10, 15, 10, 10};
const int rowLens[5] = {11, 10, 9, 10, 4};

void drawKey(int index, bool isSelected) {
    int row = 0;
    int col = 0;
    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (index >= count && index < count + rowLens[i]) {
            row = i;
            col = index - count;
            break;
        }
        count += rowLens[i];
    }

    const char* rows = isUppercase ? keyboardRowsUpper : keyboardRowsLower;
    char ch = rows[index];
    
    int startY = 110;
    int keyW = 20;
    int keyH = 26;
    int gapX = 2;
    int gapY = 4;
    
    int x, w;
    if (row == 4) {
        w = (ch == ' ') ? 70 : 45;
        if (col == 0) x = 10;
        else if (col == 1) x = 10 + 45 + 5;
        else if (col == 2) x = 10 + 45 + 5 + 70 + 5;
        else if (col == 3) x = 10 + 45 + 5 + 70 + 5 + 45 + 5;
    } else {
        x = rowStartX[row] + col * (keyW + gapX);
        w = keyW;
        if (ch == '\b') w = 35;
        else if (ch == '\t') w = 30;
    }
    
    int y = startY + row * (keyH + gapY);
    
    uint16_t keyColor = isSelected ? COLOR_GREEN : 0x2128;
    uint16_t textColor = isSelected ? 0x0000 : COLOR_WHITE;
    
    gfx->fillRoundRect(x, y, w, keyH, 4, keyColor);
    gfx->drawRoundRect(x, y, w, keyH, 4, 0x0000);
    
    gfx->setTextColor(textColor);
    gfx->setTextSize(1);
    
    if (ch == '\b') {
        gfx->setCursor(x + 8, y + 9);
        gfx->print("DEL");
    } else if (ch == '\t') {
        gfx->setCursor(x + 10, y + 9);
        gfx->print(isUppercase ? "^" : "v");
    } else if (row == 4) {
        if (ch == 'a') { gfx->setCursor(x + 12, y + 9); gfx->print("abc"); }
        else if (ch == ' ') { gfx->setCursor(x + 20, y + 9); gfx->print("space"); }
        else if (ch == '<') { gfx->setCursor(x + 18, y + 9); gfx->print("<"); }
        else if (ch == '>') { gfx->setCursor(x + 15, y + 9); gfx->print("OK"); }
    } else {
        gfx->setTextSize(2);
        gfx->setCursor(x + 5, y + 6);
        gfx->print(ch);
    }
}

void keyboardDraw(const String& title, const char* buffer, uint16_t bgColor, uint16_t headerColor, bool fullRedraw) {
    if (fullRedraw) {
        gfx->fillScreen(COLOR_LIGHT_GRAY);
        
        // Top bar
        gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_WHITE);
        gfx->drawFastHLine(0, 24, SCREEN_WIDTH, COLOR_DARK_GRAY);
        
        gfx->setTextColor(0x0000);
        gfx->setTextSize(1);
        gfx->setCursor(10, 6);
        gfx->print("User: ");
        gfx->print(deviceUsername);
        
        // Textbox
        int boxX = 8;
        int boxY = 30;
        int boxW = SCREEN_WIDTH - 16;
        int boxH = 60;
        gfx->fillRect(boxX, boxY, boxW, boxH, COLOR_WHITE);
        gfx->drawRect(boxX, boxY, boxW, boxH, COLOR_DARK_GRAY);
        
        // Keyboard Area
        int totalKeys = 0;
        for (int i = 0; i < 5; i++) totalKeys += rowLens[i];
        for (int i = 0; i < totalKeys; i++) {
            drawKey(i, i == kbdCursor);
        }
        
        // Bottom Navigation
        gfx->fillRect(0, 300, SCREEN_WIDTH, 20, 0x0000);
        gfx->setTextColor(COLOR_WHITE);
        gfx->setTextSize(1);
        gfx->setCursor(10, 306);
        gfx->print("Options");
        gfx->setCursor(110, 306);
        gfx->print("OK");
        gfx->setCursor(SCREEN_WIDTH - 40, 306);
        gfx->print("Exit");
    } else {
        // Partial redraw: only old and new key
        if (lastKbdCursor != -1 && lastKbdCursor != kbdCursor) {
            drawKey(lastKbdCursor, false);
        }
        drawKey(kbdCursor, true);
    }
    
    // Always redraw textbox content to handle cursor blink/updates
    int boxX = 8;
    int boxY = 30;
    int boxW = SCREEN_WIDTH - 16;
    int boxH = 60;
    gfx->fillRect(boxX + 2, boxY + 2, boxW - 4, boxH - 4, COLOR_WHITE);
    gfx->setTextColor(0x0000);
    gfx->setTextSize(2);
    gfx->setCursor(boxX + 5, boxY + 10);
    
    String disp = buffer;
    if (disp.length() > 30) {
        disp = "..." + disp.substring(disp.length() - 27);
    }
    gfx->print(disp);
    
    int cursorX = boxX + 5 + (disp.length() * 12);
    if (cursorX < boxX + boxW - 10) {
        if ((millis() / 500) % 2) {
            gfx->drawFastVLine(cursorX, boxY + 10, 16, 0x0000);
        }
    }
}

void keyboardHandle(int* cursor, bool btnUp, bool btnDown, bool btnLeft, bool btnRight, bool btnA, String& buffer) {
    int oldCursor = *cursor;
    
    int row = 0;
    int col = 0;
    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (*cursor >= count && *cursor < count + rowLens[i]) {
            row = i;
            col = *cursor - count;
            break;
        }
        count += rowLens[i];
    }

    if (btnUp) {
        if (row > 0) row--;
        else row = 4;
        if (col >= rowLens[row]) col = rowLens[row] - 1;
    } else if (btnDown) {
        if (row < 4) row++;
        else row = 0;
        if (col >= rowLens[row]) col = rowLens[row] - 1;
    } else if (btnLeft) {
        if (col > 0) col--;
        else col = rowLens[row] - 1;
    } else if (btnRight) {
        if (col < rowLens[row] - 1) col++;
        else col = 0;
    }

    count = 0;
    for (int i = 0; i < row; i++) count += rowLens[i];
    *cursor = count + col;

    if (btnA) {
        const char* rows = isUppercase ? keyboardRowsUpper : keyboardRowsLower;
        char ch = rows[*cursor];
        
        if (ch == '\b') { // DEL
            if (buffer.length() > 0) {
                buffer = buffer.substring(0, buffer.length() - 1);
            }
        } else if (ch == '\t') { // Shift toggle
            isUppercase = !isUppercase;
            keyboardDraw("", buffer.c_str(), 0, 0, true);
        } else if (row == 4) {
            if (ch == 'a') { // abc toggle
                isUppercase = !isUppercase;
                keyboardDraw("", buffer.c_str(), 0, 0, true);
            } else if (ch == ' ') {
                buffer += " ";
            } else if (ch == '<') {
                // Back/Huy - handled outside?
            } else if (ch == '>') {
                // OK - handled outside?
            }
        } else {
            buffer += ch;
        }
    }
}

////////////// APP PLACEHOLDERS ////////////////
void appWiFi() {
    appWiFiMenu();
}

void appSD() {
    gfx->fillScreen(COLOR_LIGHT_GRAY);
    
    // Top Bar
    gfx->fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 6);
    gfx->print("SDCARD STATUS");
    
    gfx->setCursor(185, 6);
    gfx->print("65%");
    gfx->drawRect(212, 5, 18, 10, COLOR_WHITE);
    gfx->fillRect(214, 7, 10, 6, COLOR_GREEN);
    
    // Date (Matches image)
    gfx->setTextColor(0x0000);
    gfx->setTextSize(2);
    gfx->setCursor(45, 100);
    gfx->print("23 Funr 2026");
    
    // Capacity info
    gfx->setTextSize(1);
    gfx->setCursor(55, 140);
    gfx->print("Dung luong: 32GB");
    gfx->setCursor(30, 160);
    gfx->print("(Dung: 12GB, Trong: 20GB)");
    
    // SD Card icon in middle
    int cx = SCREEN_WIDTH/2;
    int cy = 220;
    gfx->fillRoundRect(cx - 20, cy - 25, 40, 50, 4, 0x39E7);
    gfx->fillRect(cx - 15, cy - 20, 30, 10, 0xFD20);
    
    // Bottom Softkeys
    gfx->fillRect(0, 300, SCREEN_WIDTH, 20, COLOR_BG);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(5, 305);
    gfx->print("Menu");
    gfx->setCursor(SCREEN_WIDTH - 35, 305);
    gfx->print("GoTo");
    
    while (true) {
        if (buttonPressed(KEY_A)) break;
        delay(50);
    }
}

void appPaint() {
    gfx->fillScreen(COLOR_LIGHT_GRAY);
    
    // Top Bar
    gfx->fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 6);
    gfx->print("Menu Paint (v1.0)");
    
    gfx->setCursor(185, 6);
    gfx->print("65%");
    gfx->drawRect(212, 5, 18, 10, COLOR_WHITE);
    gfx->fillRect(214, 7, 10, 6, COLOR_GREEN);
    
    const char* paintItems[] = {
        "Pencil", "Brush", "Palette",
        "Text", "File", "Edit",
        "Crop", "Settings", "Exit"
    };
    
    int paintCursor = 0;
    
    auto drawPaintGrid = [&]() {
        int index = 0;
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                int x = 15 + col * 75;
                int y = 40 + row * 80;
                
                bool isSelected = (index == paintCursor);
                uint16_t bgColor = isSelected ? COLOR_SEL_BG : COLOR_LIGHT_GRAY;
                
                gfx->fillRoundRect(x, y, 60, 60, 8, bgColor);
                
                // Draw simple representative icons for paint tools
                int cx = x + 30;
                int cy = y + 25;
                if (index == 0) { // Pencil
                    gfx->drawLine(cx - 10, cy + 10, cx + 10, cy - 10, 0x0000);
                } else if (index == 1) { // Brush
                    gfx->fillRect(cx - 5, cy - 10, 10, 20, 0x8410);
                    gfx->fillCircle(cx, cy + 10, 6, 0x0000);
                } else if (index == 2) { // Palette
                    gfx->fillCircle(cx, cy, 15, 0xFFFF);
                    gfx->fillCircle(cx - 5, cy - 5, 3, COLOR_RED);
                    gfx->fillCircle(cx + 5, cy + 5, 3, COLOR_BLUE);
                }
                
                gfx->setTextColor(isSelected ? COLOR_WHITE : 0x0000);
                gfx->setTextSize(1);
                int textX = x + (60 - strlen(paintItems[index]) * 6) / 2;
                gfx->setCursor(textX, y + 65);
                gfx->print(paintItems[index]);
                
                index++;
            }
        }
        
        // Bottom Softkeys
        gfx->fillRect(0, 300, SCREEN_WIDTH, 20, COLOR_BG);
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(5, 305);
        gfx->print("Chon");
        gfx->setCursor(SCREEN_WIDTH - 35, 305);
        gfx->print("Thoat");
    };
    
    drawPaintGrid();
    
    while (true) {
        if (buttonPressed(KEY_UP)) { if (paintCursor >= 3) paintCursor -= 3; drawPaintGrid(); }
        if (buttonPressed(KEY_DOWN)) { if (paintCursor < 6) paintCursor += 3; drawPaintGrid(); }
        if (buttonPressed(KEY_LEFT)) { if (paintCursor % 3 > 0) paintCursor--; drawPaintGrid(); }
        if (buttonPressed(KEY_RIGHT)) { if (paintCursor % 3 < 2) paintCursor++; drawPaintGrid(); }
        if (buttonPressed(KEY_A)) break;
        if (buttonPressed(KEY_START)) {
            if (paintCursor == 8) break;
        }
        delay(50);
    }
}

void appCMD() {
    int kbdPos = 0;
    String cmdBuffer = "";
    keyboardDraw("CMD Terminal", cmdBuffer.c_str(), 0, 0, true);
    
    while (true) {
        bool btnUp = buttonPressed(KEY_UP);
        bool btnDown = buttonPressed(KEY_DOWN);
        bool btnLeft = buttonPressed(KEY_LEFT);
        bool btnRight = buttonPressed(KEY_RIGHT);
        bool btnA = buttonPressed(KEY_OPTION);
        bool btnB = buttonPressed(KEY_A);
        bool btnMenu = buttonPressed(KEY_MENU);
        
        if (btnB || btnMenu) break;
        
        if (btnUp || btnDown || btnLeft || btnRight || btnA) {
            keyboardHandle(&kbdPos, btnUp, btnDown, btnLeft, btnRight, btnA, cmdBuffer);
            keyboardDraw("CMD Terminal", cmdBuffer.c_str(), 0, 0, false);
        } else {
            // Just update for cursor blink
            keyboardDraw("CMD Terminal", cmdBuffer.c_str(), 0, 0, false);
        }
        delay(30);
    }
}

void appIconTest() {
    testIconDisplay();
    while (true) {
        if (buttonPressed(KEY_MENU)) {
            break;
        }
        delay(10);
    }
}

void appWEB() {
    appWeb();
}

void appScript() {
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 6);
    gfx->print("SCRIPT APP");
    
    gfx->setCursor(180, 6);
    gfx->print("65%");
    
    gfx->setTextColor(COLOR_GREEN);
    gfx->setTextSize(2);
    gfx->setCursor(40, 150);
    gfx->print("Script Editor");
    
    gfx->fillRect(0, 300, SCREEN_WIDTH, 20, COLOR_BG);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 305);
    gfx->print("Run");
    gfx->setCursor(SCREEN_WIDTH - 30, 305);
    gfx->print("Exit");
    
    while (true) {
        if (buttonPressed(KEY_A)) break;
        delay(50);
    }
}

void appRetro() {
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 6);
    gfx->print("RETRO APP");
    
    gfx->setCursor(180, 6);
    gfx->print("65%");
    
    gfx->setTextColor(COLOR_GREEN);
    gfx->setTextSize(2);
    gfx->setCursor(40, 150);
    gfx->print("Retro Games");
    
    gfx->fillRect(0, 300, SCREEN_WIDTH, 20, COLOR_BG);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 305);
    gfx->print("Options");
    gfx->setCursor(SCREEN_WIDTH - 30, 305);
    gfx->print("Exit");
    
    while (true) {
        if (buttonPressed(KEY_A)) break;
        delay(50);
    }
}

////////////// APP LIST ////////////////
const int APP_COUNT = 9;
App appList[APP_COUNT] = {
    {"WIFI", epd_bitmap_wifi, appWiFi},
    {"SDCARD", epd_bitmap_sd, appSD},
    {"PAINT", epd_bitmap_paint, appPaint},
    {"BLE", epd_bitmap_ble, appBLE},
    {"CMD", epd_bitmap_terminal, appCMD},
    {"WEB", epd_bitmap_web, appWEB},
    {"SCRIPT", epd_bitmap_script, appScript},
    {"RETRO", epd_bitmap_retro, appRetro},
    {"SETUP", epd_bitmap_setup, appSetup}
};

////////////// KERNEL ////////////////
void kernelInit() {
    gfx->begin();
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    pinMode(KEY_UP, INPUT_PULLUP);
    pinMode(KEY_DOWN, INPUT_PULLUP);
    pinMode(KEY_LEFT, INPUT_PULLUP);
    pinMode(KEY_RIGHT, INPUT_PULLUP);
    pinMode(KEY_MENU, INPUT_PULLUP);
    pinMode(KEY_OPTION, INPUT_PULLUP);
    pinMode(KEY_SELECT, INPUT_PULLUP);
    pinMode(KEY_START, INPUT_PULLUP);
    pinMode(KEY_A, INPUT_PULLUP);
    pinMode(KEY_B, INPUT_PULLUP);
    
    if (IconManager::getInstance().begin()) {
        IconManager::getInstance().loadIcons("/epd_bitmap_.bin");
    }
}

void drawStatusBar() {
    // Very Dark Gray Top Bar
    gfx->fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY);
    
    // Wi-Fi Icon (bars)
    int signalX = 5;
    int signalY = 5;
    gfx->drawFastVLine(signalX, signalY + 8, 2, COLOR_WHITE);
    gfx->drawFastVLine(signalX + 3, signalY + 6, 4, COLOR_WHITE);
    gfx->drawFastVLine(signalX + 6, signalY + 4, 6, COLOR_WHITE);
    gfx->drawFastVLine(signalX + 9, signalY + 2, 8, COLOR_WHITE);
    
    // Wi-Fi Name
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(signalX + 15, 6);
    gfx->print("ESP_S3_NET");
    
    // Time (centered)
    gfx->setCursor(105, 6);
    gfx->print("10:11");
    
    // Battery Percentage
    gfx->setCursor(185, 6);
    gfx->print("65%");
    
    // Battery Icon
    int battX = 212;
    int battY = 5;
    gfx->drawRect(battX, battY, 18, 10, COLOR_WHITE);
    gfx->fillRect(battX + 2, battY + 2, 10, 6, COLOR_GREEN); // 65% filled
    gfx->fillRect(battX + 18, battY + 3, 2, 4, COLOR_WHITE); // Tip
}

bool buttonPressed(uint8_t pin) {
    if (digitalRead(pin) == LOW) {
        delay(50);
        if (digitalRead(pin) == LOW) {
            while (digitalRead(pin) == LOW);
            return true;
        }
    }
    return false;
}

void drawRetroIcon(int index, int x, int y, int size) {
    int cx = x + size/2;
    int cy = y + size/2;
    
    switch(index) {
        case 0: // WIFI - Purple icon with white waves
            gfx->fillRoundRect(x, y, size, size, 8, 0x6296); // Purple BG
            gfx->drawCircle(cx, cy + 10, 5, COLOR_WHITE);
            gfx->drawCircle(cx, cy + 10, 10, COLOR_WHITE);
            gfx->drawCircle(cx, cy + 10, 15, COLOR_WHITE);
            gfx->fillRect(x, cy + 10, size, size/2, 0x6296); // Clip arcs
            break;
            
        case 1: // SDCARD - Realistic card shape
            gfx->fillRoundRect(x + 5, y + 2, size - 10, size - 4, 4, 0x39E7); // Gray card
            gfx->fillRect(x + 8, y + 5, size - 16, 8, 0xFD20); // Gold contacts
            gfx->setTextColor(0x0000);
            gfx->setTextSize(1);
            gfx->setCursor(x + 12, y + 20);
            gfx->print("SD");
            break;
            
        case 2: // PAINT - Palette with brush
            gfx->fillCircle(cx, cy, size/2 - 2, 0xCE79); // Palette BG
            gfx->fillCircle(cx - 8, cy - 8, 4, COLOR_RED);
            gfx->fillCircle(cx + 8, cy - 8, 4, COLOR_GREEN);
            gfx->fillCircle(cx - 8, cy + 8, 4, COLOR_BLUE);
            gfx->fillCircle(cx + 8, cy + 8, 4, COLOR_YELLOW);
            gfx->drawLine(x + 5, y + 5, x + size - 5, y + size - 5, 0x8410); // Brush handle
            gfx->fillCircle(x + size - 5, y + size - 5, 4, 0x0000); // Brush tip
            break;
            
        case 3: // BLE - Blue Bluetooth icon
            gfx->fillRoundRect(x, y, size, size, size/2, 0x041F); // Blue oval
            gfx->drawLine(cx, y + 5, cx, y + size - 5, COLOR_WHITE);
            gfx->drawLine(cx, y + 5, cx + 8, y + 12, COLOR_WHITE);
            gfx->drawLine(cx + 8, y + 12, cx - 8, y + size - 12, COLOR_WHITE);
            gfx->drawLine(cx, y + size - 5, cx + 8, y + size - 12, COLOR_WHITE);
            gfx->drawLine(cx + 8, y + size - 12, cx - 8, y + 12, COLOR_WHITE);
            break;
            
        case 4: // CMD - Console window
            gfx->fillRoundRect(x, y, size, size, 4, 0x0000); // Black BG
            gfx->drawRect(x, y, size, size, COLOR_WHITE);
            gfx->setTextColor(COLOR_GREEN);
            gfx->setTextSize(1);
            gfx->setCursor(x + 5, y + 12);
            gfx->print(">_");
            break;
            
        case 5: // WEB - Globe
            gfx->fillCircle(cx, cy, size/2 - 2, COLOR_BLUE);
            gfx->drawCircle(cx, cy, size/2 - 2, COLOR_WHITE);
            gfx->drawEllipse(cx, cy, 6, size/2 - 2, COLOR_WHITE);
            gfx->drawFastHLine(x + 2, cy, size - 4, COLOR_WHITE);
            break;
            
        case 6: // SCRIPT - Scroll
            gfx->fillRoundRect(x + 5, y + 2, size - 10, size - 4, 2, 0xFFFF);
            gfx->drawRect(x + 5, y + 2, size - 10, size - 4, 0x8410);
            gfx->drawFastHLine(x + 10, y + 10, size - 20, 0x8410);
            gfx->drawFastHLine(x + 10, y + 18, size - 20, 0x8410);
            gfx->drawFastHLine(x + 10, y + 26, size - 20, 0x8410);
            break;
            
        case 7: // RETRO - Gamepad
            gfx->fillRoundRect(x, y + 10, size, size - 20, 8, 0x8410); // Controller body
            gfx->fillRect(x + 8, y + 18, 12, 4, 0x0000); // D-pad horizontal
            gfx->fillRect(x + 12, y + 14, 4, 12, 0x0000); // D-pad vertical
            gfx->fillCircle(x + size - 10, y + 20, 4, COLOR_RED); // A button
            gfx->fillCircle(x + size - 20, y + 25, 4, COLOR_YELLOW); // B button
            break;
            
        case 8: // SETUP - Gear
            gfx->fillCircle(cx, cy, size/2 - 4, 0x8410);
            gfx->fillCircle(cx, cy, 6, COLOR_LIGHT_GRAY);
            for(int i=0; i<8; i++) {
                float ang = i * 45 * 3.14 / 180;
                gfx->fillCircle(cx + cos(ang) * (size/2 - 3), cy + sin(ang) * (size/2 - 3), 3, 0x8410);
            }
            break;
    }
}

void drawAppGrid() {
    int index = 0;
    int cols = 3;
    int iconW = 60;
    int iconH = 60;
    int startX = 15;
    int startY = 30;
    int gapX = 75;
    int gapY = 85;
    int iconSize = 40;
    
    // Light gray background for the app area (matches image)
    gfx->fillRect(0, 20, SCREEN_WIDTH, 280, COLOR_LIGHT_GRAY);
    
    gfx->setTextSize(1);
    
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < cols; col++) {
            int x = startX + col * gapX;
            int y = startY + row * gapY;
            
            bool isSelected = (index == cursorPos);
            
            // Draw background for selected item
            if (isSelected) {
                // Purple rounded rectangle selection (matches image)
                gfx->fillRoundRect(x - 5, y - 5, iconW + 10, iconH + 25, 8, COLOR_SEL_BG);
                gfx->setTextColor(COLOR_WHITE);
            } else {
                gfx->setTextColor(0x0000); // Black text for non-selected
            }
            
            int iconX = x + (iconW - iconSize) / 2;
            int iconY = y + (iconH - iconSize) / 2;
            
            // Detailed primitive icon drawing
            drawRetroIcon(index, iconX, iconY, iconSize);
            
            // App Name label below icon
            int textX = x + (iconW - strlen(appList[index].name) * 6) / 2;
            gfx->setCursor(textX, y + iconH + 5);
            gfx->print(appList[index].name);
            
            index++;
        }
    }
    
    // Bottom Softkey Bar (Black)
    gfx->fillRect(0, 300, SCREEN_WIDTH, 20, COLOR_BG);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 305);
    gfx->print("Select");
    
    gfx->setCursor(SCREEN_WIDTH - 30, 305);
    gfx->print("Exit");
}

void drawMainUI() {
    gfx->fillScreen(COLOR_LIGHT_GRAY);
    drawStatusBar();
    drawAppGrid();
}

int splashCursorY = 70;

void typeText(String txt, uint16_t color, int speed) {
    gfx->setTextColor(color);
    gfx->setCursor(15, splashCursorY);
    for (int i = 0; i < txt.length(); i++) {
        gfx->print(txt[i]);
        delay(speed);
    }
    splashCursorY += 16;
}

void drawProgress(int percent) {
    int x = 20;
    int y = 240;
    int w = 200;
    int h = 12;
    gfx->drawRect(x, y, w, h, COLOR_WHITE);
    int fill = (percent * (w - 4)) / 100;
    gfx->fillRect(x + 2, y + 2, fill, h - 4, COLOR_WHITE);
}

void bootModule(String name, int percent, uint16_t color) {
    gfx->fillRect(0, 260, SCREEN_WIDTH, 20, COLOR_BG);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    int textX = (SCREEN_WIDTH - name.length() * 6) / 2;
    gfx->setCursor(textX, 260);
    gfx->print(name);
    drawProgress(percent);
    delay(100);
}

void drawSplash() {
    gfx->fillScreen(COLOR_BG);
    
    // ModBox Logo (matches image)
    gfx->setTextSize(4);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(35, 80);
    gfx->print("ModBox");
    
    gfx->setTextSize(1);
    gfx->setCursor(35, 120);
    gfx->print("FOR ESP32-S3 Project...");
    
    gfx->setCursor(90, 210);
    gfx->print("BOOTING...");
    
    // Progress bar outline
    gfx->drawRect(20, 235, 200, 10, COLOR_WHITE);
    
    // Simulated boot modules
    bootModule("INIT KERNEL...", 15, COLOR_WHITE);
    bootModule("INIT STORAGE...", 30, COLOR_WHITE);
    bootModule("INIT DISPLAY...", 50, COLOR_WHITE);
    bootModule("INIT WIFI...", 70, COLOR_WHITE);
    bootModule("INIT BLE...", 90, COLOR_WHITE);
    bootModule("LOADED...", 100, COLOR_WHITE);
    
    // Display Admin name on boot
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(85, 280);
    gfx->print("Admin Mode");
    
    delay(500);
}

void openApp(uint8_t id) {
    systemState = SYS_APP;
    if (id < APP_COUNT && appList[id].run != NULL) {
        appList[id].run();
    }
}

void inputHandler() {
    if (systemState == SYS_MAIN) {
        if (buttonPressed(KEY_UP)) {
            cursorPos -= 3;
            if (cursorPos < 0) cursorPos = 0;
            drawMainUI();
        }
        if (buttonPressed(KEY_DOWN)) {
            cursorPos += 3;
            if (cursorPos >= APP_COUNT) cursorPos = APP_COUNT - 1;
            drawMainUI();
        }
        if (buttonPressed(KEY_LEFT)) {
            cursorPos--;
            if (cursorPos < 0) cursorPos = 0;
            drawMainUI();
        }
        if (buttonPressed(KEY_RIGHT)) {
            cursorPos++;
            if (cursorPos >= APP_COUNT) cursorPos = APP_COUNT - 1;
            drawMainUI();
        }
        if (buttonPressed(KEY_START)) {
            openApp(cursorPos);
        }
    }
    
    if (systemState == SYS_APP) {
        if (buttonPressed(KEY_A)) {
            systemState = SYS_MAIN;
            drawMainUI();
        }
    }
}

void appLoop() {
    inputHandler();
}

void systemLoop() {
    switch (systemState) {
        case SYS_BOOT:
            systemState = SYS_MAIN;
            break;
        case SYS_MAIN:
            appLoop();
            break;
        case SYS_APP:
            appLoop();
            break;
        case SYS_POPUP:
            appLoop();
            break;
    }
}
