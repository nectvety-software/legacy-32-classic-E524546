#pragma once
#include <stdint.h>

// Classic Nokia Symbian S60 light theme for the 240x320 ST7789 display (RGB565).
// White content, blue title bar, black text, light-blue selection highlight.
#define COLOR_MAIN_BACK       0xFFFF  // white content background
#define COLOR_PANEL_BACK      0x1A73  // Symbian blue title/operator bar (#1B4F9C)
#define COLOR_CARD_BACK       0xEF9F  // very light blue panel
#define COLOR_CARD_ALT        0xDF3E  // light blue-gray panel
#define COLOR_FOCUS_BACK      0xAE7E  // light selection blue highlight
#define COLOR_NAV_PANEL_BACK  0xDEFD  // light gray softkey bar
#define COLOR_BTN_BACK        0xDEFD  // light gray button
#define COLOR_MENU_ITEM       0xFFFF  // white list row

#define COLOR_ACCENT          0x1A73  // primary blue accent
#define COLOR_ACCENT_2        0x2C45  // success green
#define COLOR_TEXT_PRIMARY    0x0000  // black body text
#define COLOR_TEXT_MUTED      0x5B2E  // gray secondary text
#define COLOR_WARNING         0xD400  // amber
#define COLOR_DANGER          0xC8E3  // red
