#include "Themes.h"

// Preset Themes
const AppTheme systemThemes[] = {
    // 0: Default (Yellow) - Changed from Purple
    {"Default", COLOR_YELLOW, COLOR_YELLOW, COLOR_WHITE, COLOR_YELLOW, COLOR_BLACK},
    // 1: Matrix (Green)
    {"Matrix", COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_BLACK},
    // 2: Retro (Amber/Orange)
    {"Retro", COLOR_ORANGE, COLOR_ORANGE, COLOR_ORANGE, COLOR_ORANGE,
     COLOR_BLACK},
    // 3: Code (Custom Request)
    {"Code Style", COLOR_ORANGE, COLOR_RED, COLOR_TEAL, COLOR_LTGREEN,
     COLOR_BLACK},
    // 4: Red
    {"Red", COLOR_RED, COLOR_RED, COLOR_WHITE, COLOR_RED, COLOR_BLACK},
    // 5: Blue
    {"Blue", COLOR_BLUE, COLOR_BLUE, COLOR_WHITE, COLOR_BLUE, COLOR_BLACK},
    // 6: Yellow
    {"Yellow", COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK, COLOR_YELLOW,
     COLOR_BLACK},
    // 7: Cyan
    {"Cyan", COLOR_CYAN, COLOR_CYAN, COLOR_BLACK, COLOR_CYAN, COLOR_BLACK},
    // 8: Slate (Custom 0x596D69)
    {"Slate", COLOR_SLATE, COLOR_SLATE, COLOR_WHITE, COLOR_SLATE, COLOR_BLACK}};

const int themeCount = 9;

AppTheme currentTheme = systemThemes[0];
