/*
 * ESP32-S3 Paint Pro + Whisk3D_ESP
 * Full 2D/3D Drawing Application
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include "Whisk3D.h"

#define TFT_LED   39

#define SD_CS     10
#define SD_MOSI   11
#define SD_MISO   13
#define SD_SCK    14

#define KEY_UP      7
#define KEY_DOWN    46
#define KEY_LEFT    45
#define KEY_RIGHT   6
#define KEY_MENU    18
#define KEY_OPTION  8
#define KEY_SELECT  16
#define KEY_START   17
#define KEY_A       15
#define KEY_B       5

#define SCREEN_W    240
#define SCREEN_H    320
#define CANVAS_X     0
#define CANVAS_Y     25
#define CANVAS_W     240
#define CANVAS_H     270

#define UI_BG       0x2124
#define UI_SEL      0x001F
#define UI_TEXT     0xFFFF
#define UI_GRAY     0x8410
#define UI_ARROW    0xFBE0
#define UI_HEADER   0x1082
#define UI_BAR      0x0000
#define UI_PANEL    0x18E3

TFT_eSPI tft = TFT_eSPI();
Whisk3D* w3d = nullptr;

enum AppMode { MODE_HOME, MODE_PAINT, MODE_3D, MODE_SD };
enum PaintTool { TOOL_PEN, TOOL_BRUSH, TOOL_LINE, TOOL_RECT, TOOL_CIRCLE, TOOL_FILL, TOOL_TEXT, TOOL_ERASER, TOOL_SOFTBRUSH };

AppMode appMode = MODE_HOME;
PaintTool currentTool = TOOL_PEN;
bool inMenu = false;
bool isDrawing = false;
bool drawingActive = false;
int brushSize = 3;
uint16_t currentColor = 0xFFFF;

int lastX = -1, lastY = -1;
int drawStartX = -1, drawStartY = -1;
bool canvasDirty = false;

int rVal = 255, gVal = 255, bVal = 255;

struct Button {
    uint8_t pin;
    bool pressed;
    bool lastState;
} buttons[10];

struct DrawingLayer {
    bool active;
    uint16_t color;
    int brushSize;
};
DrawingLayer layers[4] = {{true, 0xFFFF, 3}, {false, 0xFFFF, 3}, {false, 0xFFFF, 3}, {false, 0xFFFF, 3}};
int currentLayer = 0;

char saveFormat = 'B';
bool sdAvailable = false;
char currentFolder[64] = "/";

void drawPixelW3D(int16_t x, int16_t y, uint16_t color) {
    tft.drawPixel(x, y, color);
}

void fillRectW3D(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}

void initButtons() {
    int pins[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_MENU, 
                  KEY_OPTION, KEY_SELECT, KEY_START, KEY_A, KEY_B};
    for (int i = 0; i < 10; i++) {
        buttons[i].pin = pins[i];
        buttons[i].pressed = false;
        buttons[i].lastState = true;
        pinMode(pins[i], INPUT_PULLUP);
    }
}

void initSD() {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sdAvailable = SD.begin(SD_CS);
    if (sdAvailable) {
        Serial.println("SD Card OK");
    } else {
        Serial.println("SD Card Error");
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("PaintPro starting...");
    
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);
    
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    Serial.println("Display OK");
    
    initButtons();
    initSD();
    
    drawHomeScreen();
}

void loop() {
    if (appMode == MODE_PAINT && !inMenu) {
        handlePaintDrawing();
    }
    
    for (int i = 0; i < 10; i++) {
        bool state = !digitalRead(buttons[i].pin);
        if (state && !buttons[i].lastState) {
            handleButtonPress(i);
            buttons[i].lastState = true;
        } else if (!state && buttons[i].lastState) {
            buttons[i].lastState = false;
        }
    }
    delay(30);
}

void handlePaintDrawing() {
    static int drawX = SCREEN_W / 2;
    static int drawY = CANVAS_Y + CANVAS_H / 2;
    static bool drawing = false;
    
    bool aPressed = !digitalRead(KEY_START);
    bool upPressed = !digitalRead(KEY_UP);
    bool downPressed = !digitalRead(KEY_DOWN);
    bool leftPressed = !digitalRead(KEY_LEFT);
    bool rightPressed = !digitalRead(KEY_RIGHT);
    
    if (aPressed && !drawing) {
        drawing = true;
        lastX = drawX;
        lastY = drawY;
    } else if (!aPressed && drawing) {
        drawing = false;
    }
    
    int speed = 2;
    if (upPressed || downPressed || leftPressed || rightPressed) {
        if (upPressed) drawY = max(CANVAS_Y, drawY - speed);
        if (downPressed) drawY = min(CANVAS_Y + CANVAS_H, drawY + speed);
        if (leftPressed) drawX = max(CANVAS_X, drawX - speed);
        if (rightPressed) drawX = min(CANVAS_X + CANVAS_W, drawX + speed);
        
        if (drawing) {
            if (currentTool == TOOL_SOFTBRUSH) {
                drawBrushStroke(drawX, drawY, lastX, lastY, currentColor, brushSize, true);
            } else if (currentTool == TOOL_BRUSH) {
                drawBrushStroke(drawX, drawY, lastX, lastY, currentColor, brushSize, false);
            } else if (currentTool == TOOL_PEN) {
                tft.drawPixel(drawX, drawY, currentColor);
            } else if (currentTool == TOOL_ERASER) {
                tft.fillCircle(drawX, drawY, brushSize, 0x1082);
            }
            lastX = drawX;
            lastY = drawY;
        }
        
        tft.fillRect(SCREEN_W - 50, 0, 50, 25, UI_HEADER);
        tft.setTextColor(UI_TEXT);
        tft.setCursor(SCREEN_W - 45, 8);
        tft.print(drawX);
        tft.print(",");
        tft.print(drawY);
    }
}

void handleButtonPress(int btn) {
    const char* names[] = {"UP", "DOWN", "LEFT", "RIGHT", "MENU", "OPT", "SEL", "STA", "A", "B"};
    Serial.println(names[btn]);
    
    if (appMode == MODE_HOME) {
        handleHomePress(btn);
    } else if (appMode == MODE_PAINT) {
        handlePaintPress(btn);
    } else if (appMode == MODE_3D) {
        handle3DPress(btn);
    } else if (appMode == MODE_SD) {
        handleSDPress(btn);
    }
}

void handleHomePress(int btn) {
    static int selected = 0;
    const char* items[] = {"2D Paint", "3D whisk3D", "SD Card", "Exit"};
    int count = 4;
    
    if (btn == 0) { selected = (selected - 1 + count) % count; drawHomeScreen(); }
    else if (btn == 1) { selected = (selected + 1) % count; drawHomeScreen(); }
    else if (btn == 7) {
        switch(selected) {
            case 0: appMode = MODE_PAINT; drawPaintScreen(); break;
            case 1: appMode = MODE_3D; init3DMode(); break;
            case 2: appMode = MODE_SD; drawSDScreen(); break;
            case 3: drawHomeScreen(); break;
        }
    } else if (btn == 4 || btn == 8) {
        drawHomeScreen();
    }
}

void drawHomeScreen() {
    tft.fillScreen(UI_BG);
    
    tft.fillRect(0, 0, SCREEN_W, 40, UI_HEADER);
    tft.setTextColor(UI_ARROW);
    tft.setTextSize(2);
    tft.setCursor(60, 10);
    tft.print("PaintPro");
    
    tft.setTextSize(1);
    const char* items[] = {"2D Paint", "3D whisk3D", "SD Card", "Exit"};
    for (int i = 0; i < 4; i++) {
        int y = 60 + i * 45;
        tft.fillRect(20, y, SCREEN_W - 40, 40, i == 0 ? UI_PANEL : UI_BG);
        tft.setTextColor(i == 0 ? UI_BG : UI_TEXT);
        tft.setCursor(35, y + 12);
        tft.print(items[i]);
    }
    
    tft.fillRect(0, SCREEN_H - 25, SCREEN_W, 25, UI_BAR);
    tft.setTextColor(UI_GRAY);
    tft.setCursor(10, SCREEN_H - 18);
    tft.print("UP/DOWN: Select | OK: Enter");
}

void handlePaintPress(int btn) {
    static int toolSelected = 0;
    
    if (btn == 4) {
        drawPaintMenu();
    } else if (btn == 0) {
        if (toolSelected > 0) toolSelected--;
        drawToolBar();
    } else if (btn == 1) {
        if (toolSelected < 8) toolSelected++;
        drawToolBar();
    } else if (btn == 5) {
        if (!inMenu) drawColorPicker();
    } else if (btn == 7) {
        if (inMenu) {
            if (toolSelected == 6) drawTextInput();
            else inMenu = false;
        }
    } else if (btn == 8) {
        executeTool(toolSelected);
    } else if (btn == 2) {
        appMode = MODE_HOME;
        drawHomeScreen();
    }
}

void drawPaintScreen() {
    tft.fillScreen(UI_BG);
    
    tft.fillRect(0, 0, SCREEN_W, 25, UI_HEADER);
    tft.setTextColor(UI_TEXT);
    tft.setTextSize(1);
    tft.setCursor(5, 5);
    tft.print("2D Paint");
    
    tft.fillRect(SCREEN_W - 40, 2, 35, 20, currentColor);
    tft.setTextColor(UI_GRAY);
    tft.setCursor(SCREEN_W - 38, 6);
    tft.print("RGB");
    
    tft.fillRect(CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_H, 0x1082);
    drawGrid();
    
    drawToolBar();
    
    tft.fillRect(0, SCREEN_H - 25, SCREEN_W, 25, UI_BAR);
    tft.setTextColor(UI_TEXT);
    tft.setCursor(5, SCREEN_H - 18);
    tft.print("MENU");
    tft.setCursor(90, SCREEN_H - 18);
    tft.print("COLOR");
}

void drawToolBar() {
    tft.fillRect(0, CANVAS_Y + CANVAS_H, SCREEN_W, 25, UI_BAR);
    const char* tools[] = {"Pen", "Br", "Li", "Re", "Ci", "Fi", "Te", "Er", "SB"};
    int toolCount = 9;
    for (int i = 0; i < toolCount; i++) {
        int x = i * 26;
        if (i == currentTool) {
            tft.fillRect(x + 2, CANVAS_Y + CANVAS_H + 2, 24, 21, UI_SEL);
            tft.setTextColor(UI_TEXT);
        } else {
            tft.fillRect(x + 2, CANVAS_Y + CANVAS_H + 2, 24, 21, UI_BG);
            tft.setTextColor(UI_GRAY);
        }
        tft.setTextSize(1);
        tft.setCursor(x + 5, CANVAS_Y + CANVAS_H + 7);
        tft.print(tools[i]);
    }
}

void drawGrid() {
    tft.drawRect(CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_H, UI_GRAY);
}

void drawSoftBrush(int x, int y, uint16_t color, int size) {
    int r = size * 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist <= r) {
                int alpha = map(dist, 0, r, 255, 50);
                alpha = constrain(alpha, 50, 255);
                
                uint8_t existingR = (tft.readPixel(x + dx, y + dy) >> 11) & 0x1F;
                uint8_t existingG = (tft.readPixel(x + dx, y + dy) >> 5) & 0x3F;
                uint8_t existingB = tft.readPixel(x + dx, y + dy) & 0x1F;
                
                uint8_t newR = (color >> 11) & 0x1F;
                uint8_t newG = (color >> 5) & 0x3F;
                uint8_t newB = color & 0x1F;
                
                float blend = alpha / 255.0f;
                uint8_t blendR = (existingR * (1 - blend) + newR * blend);
                uint8_t blendG = (existingG * (1 - blend) + newG * blend);
                uint8_t blendB = (existingB * (1 - blend) + newB * blend);
                
                uint16_t blended = (blendR << 11) | (blendG << 5) | blendB;
                tft.drawPixel(x + dx, y + dy, blended);
            }
        }
    }
}

void drawBrushStroke(int x, int y, int lastX, int lastY, uint16_t color, int size, bool soft) {
    if (lastX < 0 || lastY < 0) {
        if (soft) {
            drawSoftBrush(x, y, color, size);
        } else {
            tft.fillCircle(x, y, size, color);
        }
        return;
    }
    
    int dx = abs(x - lastX);
    int dy = abs(y - lastY);
    int sx = lastX < x ? 1 : -1;
    int sy = lastY < y ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        if (x >= CANVAS_X && x < CANVAS_X + CANVAS_W && y >= CANVAS_Y && y < CANVAS_Y + CANVAS_H) {
            if (soft) {
                drawSoftBrush(x, y, color, size);
            } else {
                tft.fillCircle(x, y, size, color);
            }
        }
        
        if (x == lastX && y == lastY) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

void executeTool(int tool) {
    currentTool = (PaintTool)tool;
    drawToolBar();
    
    if (tool == TOOL_FILL) {
        tft.fillRect(CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_H, currentColor);
    } else if (tool == TOOL_SOFTBRUSH) {
        tft.fillRect(0, SCREEN_H - 25, SCREEN_W, 25, UI_BAR);
        tft.setTextColor(UI_TEXT);
        tft.setCursor(5, SCREEN_H - 18);
        tft.print("Soft Brush: A+Dir");
    }
}

void drawPaintMenu() {
    tft.fillScreen(UI_BG);
    
    tft.fillRect(0, 0, SCREEN_W, 30, UI_HEADER);
    tft.setTextColor(UI_ARROW);
    tft.setTextSize(1);
    tft.setCursor(80, 8);
    tft.print("TOOLS MENU");
    
    const char* items[] = {"Brush Size", "Color RGB", "Save Image", "Load Image", "Clear Canvas", "Back"};
    for (int i = 0; i < 6; i++) {
        int y = 45 + i * 35;
        tft.fillRect(10, y, SCREEN_W - 20, 30, UI_PANEL);
        tft.setTextColor(UI_TEXT);
        tft.setCursor(20, y + 8);
        tft.print(items[i]);
    }
    
    static int sel = 0;
    tft.fillRect(5, 45 + sel * 35, 5, 30, UI_ARROW);
    
    int btn = waitForButton();
    if (btn == 0) { sel = (sel - 1 + 6) % 6; drawPaintMenu(); }
    else if (btn == 1) { sel = (sel + 1) % 6; drawPaintMenu(); }
    else if (btn == 7) {
        switch(sel) {
            case 0: drawBrushSizeMenu(); break;
            case 1: drawColorPicker(); break;
            case 2: showSaveDialog(); break;
            case 3: showLoadDialog(); break;
            case 4: tft.fillRect(CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_H, 0x1082); drawPaintScreen(); break;
            case 5: drawPaintScreen(); break;
        }
    }
}

void drawBrushSizeMenu() {
    tft.fillScreen(UI_BG);
    tft.setTextColor(UI_ARROW);
    tft.setCursor(70, 50);
    tft.print("BRUSH SIZE");
    
    tft.setTextColor(UI_TEXT);
    for (int i = 1; i <= 20; i++) {
        int x = 10 + (i - 1) % 10 * 22;
        int y = 100 + (i - 1) / 10 * 40;
        tft.fillCircle(x + 10, y, i, i == brushSize ? UI_ARROW : UI_GRAY);
        tft.setTextColor(UI_TEXT);
        tft.setCursor(x + 5, y + 25);
        tft.print(i);
    }
    
    static int sel = 1;
    if (sel != brushSize) {
        tft.drawCircle(10 + (sel - 1) % 10 * 22 + 10, 100 + (sel - 1) / 10 * 40, sel, UI_TEXT);
        tft.drawCircle(10 + (brushSize - 1) % 10 * 22 + 10, 100 + (brushSize - 1) / 10 * 40, brushSize, UI_ARROW);
        sel = brushSize;
    }
    
    int btn = waitForButton();
    if (btn == 0) { sel = (sel - 1 + 20) % 20 + 1; drawBrushSizeMenu(); }
    else if (btn == 1) { sel = (sel % 20) + 1; drawBrushSizeMenu(); }
    else if (btn == 7) { brushSize = sel; drawPaintMenu(); }
    else if (btn == 2) { drawPaintMenu(); }
}

void drawColorPicker() {
    tft.fillScreen(UI_BG);
    
    tft.setTextColor(UI_ARROW);
    tft.setCursor(70, 10);
    tft.print("COLOR PICKER");
    
    tft.fillRect(10, 35, 100, 100, tft.color565(rVal, gVal, bVal));
    tft.drawRect(10, 35, 100, 100, UI_TEXT);
    
    tft.setTextColor(UI_TEXT);
    tft.setCursor(10, 145);
    tft.printf("R:%d G:%d B:%d", rVal, gVal, bVal);
    
    tft.setTextColor(UI_GRAY);
    tft.setCursor(10, 170);
    tft.print("UP/DOWN: +/-");
    tft.setCursor(10, 185);
    tft.print("LEFT/RIGHT: R-G-B");
    tft.setCursor(10, 200);
    tft.print("A: Confirm");
    
    static int selColor = 0;
    static bool redraw = true;
    if (redraw) {
        tft.fillRect(10, 220, 30, 20, selColor == 0 ? UI_ARROW : UI_BG);
        tft.fillRect(50, 220, 30, 20, selColor == 1 ? UI_ARROW : UI_BG);
        tft.fillRect(90, 220, 30, 20, selColor == 2 ? UI_ARROW : UI_BG);
        tft.setTextColor(UI_TEXT);
        tft.setCursor(15, 225);
        tft.print("R");
        tft.setCursor(55, 225);
        tft.print("G");
        tft.setCursor(95, 225);
        tft.print("B");
        redraw = false;
    }
    
    int btn = waitForButton();
    if (btn == 0) {
        if (selColor == 0) rVal = min(255, rVal + 15);
        else if (selColor == 1) gVal = min(255, gVal + 15);
        else bVal = min(255, bVal + 15);
        redraw = true;
    } else if (btn == 1) {
        if (selColor == 0) rVal = max(0, rVal - 15);
        else if (selColor == 1) gVal = max(0, gVal - 15);
        else bVal = max(0, bVal - 15);
        redraw = true;
    } else if (btn == 2) { selColor = (selColor + 2) % 3; redraw = true; }
    else if (btn == 3) { selColor = (selColor + 1) % 3; redraw = true; }
    else if (btn == 7) { currentColor = tft.color565(rVal, gVal, bVal); drawPaintMenu(); }
    else if (btn == 2) { drawPaintMenu(); }
    
    if (redraw) drawColorPicker();
}

void showSaveDialog() {
    tft.fillScreen(UI_BG);
    tft.setTextColor(UI_ARROW);
    tft.setCursor(60, 20);
    tft.print("SAVE IMAGE");
    
    tft.setTextColor(UI_TEXT);
    tft.setCursor(10, 60);
    tft.print("Format:");
    
    const char* formats[] = {"BMP", "PNG", "JPG"};
    for (int i = 0; i < 3; i++) {
        int x = 20 + i * 70;
        tft.fillRect(x, 80, 60, 30, saveFormat == formats[i][0] ? UI_SEL : UI_PANEL);
        tft.setTextColor(saveFormat == formats[i][0] ? UI_BG : UI_TEXT);
        tft.setCursor(x + 15, 90);
        tft.print(formats[i]);
    }
    
    char filename[32];
    sprintf(filename, "IMG_%04d", random(1000, 9999));
    tft.setTextColor(UI_TEXT);
    tft.setCursor(10, 130);
    tft.print("Name: ");
    tft.print(filename);
    
    tft.setTextColor(UI_GRAY);
    tft.setCursor(10, 180);
    tft.print("Folder: ");
    tft.print(currentFolder);
    
    if (sdAvailable) {
        tft.setCursor(10, 220);
        tft.print("SD: OK");
        tft.setCursor(10, 240);
        tft.print("Press A to save");
    } else {
        tft.setTextColor(TFT_RED);
        tft.setCursor(10, 220);
        tft.print("SD: Not found!");
    }
    
    static int selFormat = 0;
    int btn = waitForButton();
    if (btn == 2) { selFormat = (selFormat + 2) % 3; saveFormat = formats[selFormat][0]; showSaveDialog(); }
    else if (btn == 3) { selFormat = (selFormat + 1) % 3; saveFormat = formats[selFormat][0]; showSaveDialog(); }
    else if (btn == 7 && sdAvailable) {
        saveImage(filename);
        drawPaintMenu();
    } else if (btn == 2) { drawPaintMenu(); }
}

void saveImage(const char* name) {
    char filepath[64];
    sprintf(filepath, "%s/%s.%c", currentFolder, name, saveFormat);
    
    File file = SD.open(filepath, FILE_WRITE);
    if (file) {
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(10, 260);
        tft.print("Saved!");
        file.close();
    } else {
        tft.setTextColor(TFT_RED);
        tft.setCursor(10, 260);
        tft.print("Save failed!");
    }
    delay(1000);
}

void showLoadDialog() {
    tft.fillScreen(UI_BG);
    tft.setTextColor(UI_ARROW);
    tft.setCursor(60, 10);
    tft.print("LOAD IMAGE");
    
    if (!sdAvailable) {
        tft.setTextColor(TFT_RED);
        tft.setCursor(20, 100);
        tft.print("SD Card not found!");
        delay(2000);
        drawPaintMenu();
        return;
    }
    
    File root = SD.open(currentFolder);
    int fileCount = 0;
    char files[20][32];
    
    if (root) {
        while (File entry = root.openNextFile()) {
            String name = entry.name();
            if (!entry.isDirectory() && (name.endsWith(".bmp") || name.endsWith(".png") || name.endsWith(".jpg"))) {
                name.toCharArray(files[fileCount], 32);
                fileCount++;
                if (fileCount >= 20) break;
            }
        }
        root.close();
    }
    
    if (fileCount == 0) {
        tft.setTextColor(UI_TEXT);
        tft.setCursor(20, 80);
        tft.print("No images found");
    } else {
        tft.setTextColor(UI_TEXT);
        tft.setCursor(10, 40);
        tft.printf("%d images found", fileCount);
        
        static int sel = 0;
        for (int i = 0; i < min(fileCount, 8); i++) {
            int idx = (sel / 8) * 8 + i;
            if (idx < fileCount) {
                int y = 60 + i * 25;
                tft.fillRect(10, y, SCREEN_W - 20, 22, i == (sel % 8) ? UI_SEL : UI_PANEL);
                tft.setTextColor(i == (sel % 8) ? UI_BG : UI_TEXT);
                tft.setCursor(20, y + 5);
                tft.print(files[idx]);
            }
        }
        
        int btn = waitForButton();
        if (btn == 0) { sel = (sel - 1 + fileCount) % fileCount; showLoadDialog(); }
        else if (btn == 1) { sel = (sel + 1) % fileCount; showLoadDialog(); }
        else if (btn == 7) {
            tft.fillRect(0, CANVAS_Y, CANVAS_W, CANVAS_H, 0x1082);
            tft.setTextColor(TFT_GREEN);
            tft.setCursor(60, CANVAS_Y + 100);
            tft.print(files[sel]);
            delay(1500);
            drawPaintScreen();
        } else if (btn == 2) { drawPaintMenu(); }
    }
}

void drawTextInput() {
    tft.fillScreen(UI_BG);
    tft.setTextColor(UI_ARROW);
    tft.setCursor(70, 20);
    tft.print("TEXT INPUT");
    
    tft.setTextColor(UI_TEXT);
    tft.setCursor(20, 80);
    tft.print("Coming soon...");
    delay(1500);
    drawPaintMenu();
}

int waitForButton() {
    while (true) {
        for (int i = 0; i < 10; i++) {
            if (!digitalRead(buttons[i].pin)) return i;
        }
        delay(50);
    }
}

void init3DMode() {
    if (w3d) delete w3d;
    w3d = new Whisk3D(SCREEN_W, SCREEN_H);
    w3d->setDisplayCallbacks(drawPixelW3D, fillRectW3D);
    w3d->setCamera(Vector3(0, 2, 5), Vector3(0, 0, 0), Vector3(0, 1, 0));
    draw3DScreen();
}

void draw3DScreen() {
    tft.fillScreen(0x0000);
    
    w3d->clear(0x1082);
    w3d->drawGrid(6, 6, 0x3186);
    
    static float rotY = 0;
    Object3D cube;
    cube.position = Vector3(0, 0, 0);
    cube.rotation = Vector3(0.3, rotY, 0);
    cube.scale = Vector3(1, 1, 1);
    
    float size = 1.0f;
    float h = size / 2.0f;
    Vertex verts[8] = {
        {Vector3(-h, -h, -h), 0xF800},
        {Vector3(h, -h, -h), 0x07E0},
        {Vector3(h, h, -h), 0x001F},
        {Vector3(-h, h, -h), 0xFFE0},
        {Vector3(-h, -h, h), 0xF81F},
        {Vector3(h, -h, h), 0x07FF},
        {Vector3(h, h, h), 0xFFFF},
        {Vector3(-h, h, h), 0xF044}
    };
    uint16_t inds[] = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        2, 6, 7, 2, 7, 3,
        0, 3, 7, 0, 7, 4,
        1, 5, 6, 1, 6, 2
    };
    cube.vertices = verts;
    cube.vertexCount = 8;
    cube.indices = inds;
    cube.indexCount = 36;
    
    w3d->drawCube(1.5f, 0xF800);
    w3d->drawPyramid(1.0f, 1.2f, 0x07E0);
    
    rotY += 0.02f;
    
    tft.setTextColor(UI_TEXT);
    tft.setTextSize(1);
    tft.setCursor(5, 5);
    tft.print("3D Mode");
    tft.setCursor(5, 15);
    tft.print("whisk3D");
}

void handle3DPress(int btn) {
    if (btn == 2 || btn == 8) {
        appMode = MODE_HOME;
        drawHomeScreen();
    }
}

void drawSDScreen() {
    tft.fillScreen(UI_BG);
    tft.setTextColor(UI_ARROW);
    tft.setCursor(70, 10);
    tft.print("SD CARD");
    
    if (!sdAvailable) {
        tft.setTextColor(TFT_RED);
        tft.setCursor(20, 100);
        tft.print("SD Card Error!");
        delay(2000);
        appMode = MODE_HOME;
        drawHomeScreen();
        return;
    }
    
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(10, 40);
    tft.print("SD: OK");
    
    File root = SD.open("/");
    int fileCount = 0;
    char files[20][64];
    
    if (root) {
        while (File entry = root.openNextFile()) {
            String name = entry.name();
            if (!entry.isDirectory()) {
                name.toCharArray(files[fileCount], 64);
                fileCount++;
                if (fileCount >= 20) break;
            }
        }
        root.close();
    }
    
    tft.setTextColor(UI_TEXT);
    tft.setCursor(10, 60);
    tft.printf("%d files", fileCount);
    
    static int sel = 0;
    for (int i = 0; i < min(fileCount, 10); i++) {
        int y = 80 + i * 22;
        bool isSelected = (i == sel % fileCount);
        tft.fillRect(10, y, SCREEN_W - 20, 20, isSelected ? UI_SEL : UI_PANEL);
        tft.setTextColor(isSelected ? UI_BG : UI_TEXT);
        tft.setCursor(20, y + 4);
        tft.print(files[(sel / 10) * 10 + i]);
    }
    
    int btn = waitForButton();
    if (btn == 0) { sel = (sel - 1 + fileCount) % max(1, fileCount); drawSDScreen(); }
    else if (btn == 1) { sel = (sel + 1) % max(1, fileCount); drawSDScreen(); }
    else if (btn == 2 || btn == 8) { appMode = MODE_HOME; drawHomeScreen(); }
    else if (btn == 7 && fileCount > 0) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UI_TEXT);
        tft.setCursor(30, 150);
        tft.print(files[sel]);
        delay(2000);
        drawSDScreen();
    }
}

void handleSDPress(int btn) {
    if (btn == 2 || btn == 8) {
        appMode = MODE_HOME;
        drawHomeScreen();
    }
}
