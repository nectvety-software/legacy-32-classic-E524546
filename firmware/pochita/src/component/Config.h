#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include "BoardPins.h"
#include "Display.h"
#include "UILayout.h"
#include "SymbianUI.h"

// External Global Objects
extern TFT_eSPI tft;
extern SPIClass sdSPI;
bool mountSDCard(bool formatIfEmpty = false,
                 uint32_t frequency = 20000000);

#include "Themes.h"

// Colors
extern AppTheme currentTheme;
#define THEME_COLOR currentTheme.primary
#define TEXT_COLOR currentTheme.text
#define HL_COLOR currentTheme.highlight
#define BG_COLOR currentTheme.background

// Enums
// App Forward Declarations
void drawLauncherContent();
void drawPowerMenu();
void exitAppToHome(const char* extMsg = nullptr);
// void drawNotesApp();
// void drawShellApp();
// void drawFileManager();
// void drawRouter();

// Enums
enum SystemMode {
  MODE_HOME,
  MODE_WIFI_MAIN,
  MODE_WIFI_SCAN,
  MODE_WIFI_SAVED,
  MODE_WIFI_PROPERTIES,
  MODE_KEYBOARD_INPUT,
  MODE_FILE_MANAGER,
  MODE_NOTES, 
  MODE_TERMINAL,
  MODE_LAUNCHER,
  MODE_SETTINGS,
  MODE_POWER_OFF,
  MODE_SPLASH,
  MODE_POWER_MENU,
  MODE_APP_ROUTER,
  MODE_APP_SDCARD,
  MODE_APP_BLU,
  MODE_APP_SHELL,
  MODE_APP_NOTES,
  MODE_APP_MAPS,
  MODE_APP_WEBS,
  MODE_APP_IR,
  MODE_APP_BROWSER,
  MODE_APP_SETTINGS,
  MODE_APP_RADIO,
  MODE_APP_MUSIC,
  MODE_APP_PAINT,
  MODE_APP_RETRO,
  MODE_APP_CHAT,
  MODE_APP_VM
};

extern SystemMode currentMode;

#endif
