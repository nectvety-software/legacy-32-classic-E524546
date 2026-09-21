#ifndef MODBOX_MAIN_H
#define MODBOX_MAIN_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

//////////////// CONFIG ////////////////

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

//////////////// TFT PIN ////////////////

#define TFT_BL   39
#define TFT_DC   47
#define TFT_CS   14
#define TFT_SCLK 48
#define TFT_MOSI 12
#define TFT_RST  3

//////////////// BUTTON PIN ////////////////

#define KEY_UP 7
#define KEY_DOWN 46
#define KEY_LEFT 45
#define KEY_RIGHT 6
#define KEY_MENU 18
#define KEY_OPTION 8
#define KEY_SELECT 16
#define KEY_START 17
#define KEY_A 15
#define KEY_B 5

//////////////// COLOR ////////////////

#define COLOR_BG        0x0000 // Black
#define COLOR_WHITE     0xFFFF // White
#define COLOR_YELLOW    0xFD20
#define COLOR_GREEN     0x07E0
#define COLOR_RED       0xF800
#define COLOR_BLUE      0x041F
#define COLOR_GRAY      0x39E7
#define COLOR_DARK_GRAY 0x2124 // Material Retro Dark Gray (Status bars)
#define COLOR_LIGHT_GRAY 0xCE79 // Material Retro Light Gray (Backgrounds)
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_KEY_BG    0x1A94 // Retro Blue for keys
#define COLOR_SEL_BG    0x5294 // Purple-ish Blue for selection
#define COLOR_TAB_BG    0x2128
#define COLOR_SOFTKEY   0x2124

//////////////// SYSTEM STATE ////////////////

enum SystemState
{
SYS_BOOT,
SYS_MAIN,
SYS_APP,
SYS_POPUP
};

extern SystemState systemState;

//////////////// APP STRUCT ////////////////

struct App
{
const char* name;
const uint16_t* icon;
void (*run)();
};

//////////////// GLOBAL ////////////////

extern Arduino_GFX *gfx;
extern int cursorPos;
extern App appList[9];
extern String deviceUsername;

//////////////// KERNEL ////////////////

void kernelInit();

//////////////// UI ////////////////

void drawSplash();
void drawMainUI();
void drawStatusBar();
void drawAppGrid();
void typeText(String txt, uint16_t color, int speed);
void drawProgress(int percent);
void bootModule(String name, int percent, uint16_t color);

//////////////// INPUT ////////////////

bool buttonPressed(uint8_t pin);
void inputHandler();

//////////////// SHARED KEYBOARD ////////////////

extern const char* keyboardRowsUpper;
extern const char* keyboardRowsLower;
extern int kbdCursor;
extern int lastKbdCursor;
extern bool isUppercase;
extern const int rowStartX[5];
extern const int rowLens[5];

void drawKey(int index, bool isSelected);
void keyboardDraw(const String& title, const char* buffer, uint16_t bgColor, uint16_t headerColor, bool fullRedraw);
void keyboardHandle(int* cursor, bool btnUp, bool btnDown, bool btnLeft, bool btnRight, bool btnA, String& buffer);

//////////////// APP ////////////////

void openApp(uint8_t id);
void appLoop();

//////////////// SYSTEM ////////////////

void systemLoop();

#include "ble.h"
#include "web.h"
#include "wifi_app.h"
#include "icon_manager.h"
#include "setup_manager.h"
#include "cmd_app.h"

#endif
