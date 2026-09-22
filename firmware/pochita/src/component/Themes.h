#ifndef THEMES_H
#define THEMES_H

#include <Arduino.h>

// RGB565 Color Definitions
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_PURPLE 0xFFE0 // Yellow color instead
#define COLOR_CYAN 0x07FF
#define COLOR_ORANGE 0xFD20
#define COLOR_WHITE 0xFFFF
#define COLOR_BLACK 0x0000
#define COLOR_TEAL 0x0410 // Dark Cyan/Greenish
#define COLOR_LTGREEN 0x87F0
#define COLOR_SLATE 0x5B6D // 0x596D69 (Custom Request)

struct AppTheme {
  const char *name;
  uint16_t primary;    // Frame, Headers
  uint16_t startColor; // Start Button (reserved/unused for now)
  uint16_t text;       // Normal text
  uint16_t highlight;  // Selected Item Background
  uint16_t background; // Main Background

  // Default constructor
  AppTheme()
      : name(""), primary(0), startColor(0), text(0), highlight(0),
        background(0) {}

  // Config constructor
  AppTheme(const char *n, uint16_t p, uint16_t s, uint16_t t, uint16_t h,
           uint16_t b)
      : name(n), primary(p), startColor(s), text(t), highlight(h),
        background(b) {}

  // Allow assignment from another AppTheme
  AppTheme &operator=(const AppTheme &other) {
    if (this != &other) {
      name = other.name;
      primary = other.primary;
      startColor = other.startColor;
      text = other.text;
      highlight = other.highlight;
      background = other.background;
    }
    return *this;
  }
};

// Preset Themes Extern Declaration
extern const AppTheme systemThemes[];
extern const int themeCount;

#endif
