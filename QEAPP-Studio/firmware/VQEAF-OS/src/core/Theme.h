#pragma once
#include <Arduino.h>

// Built-in themes are compiled as RGB565 constants so changing appearance costs
// no heap and never requires a full-screen bitmap on the 240x320 ST7789.
// v1.1 adds AMOLED Red, adapted from VQEAF Theme Studio's amoled_red.vqeaf.
struct ThemeColors {
  uint16_t bg;
  uint16_t panel;
  uint16_t selected;
  uint16_t text;
  uint16_t dim;
  uint16_t chrome;
  uint16_t chromeText;
  uint16_t accent;
  uint16_t danger;
  uint16_t popup;
  uint16_t popupText;
  uint16_t popupSelected;
  uint16_t border;
};

enum class ThemeId : uint8_t {
  Classic = 0,
  Black = 1,
  S60Green = 2,
  AmoledRed = 3,
  External = 4
};

inline const char *themeName(ThemeId id) {
  switch (id) {
    case ThemeId::S60Green: return "VQEAF Lime";
    case ThemeId::AmoledRed: return "AMOLED Red";
    case ThemeId::Black: return "Black";
    case ThemeId::External: return "SD card theme";
    default: return "VQEAF Night";
  }
}

inline ThemeColors themeFor(ThemeId id) {
  if (id == ThemeId::S60Green) {
    return {
      0x8E44, // bg - lime green
      0xAEE9, // panel - light green
      0xDF93, // selected - pale green
      0x0000, // text
      0x2222, // dim
      0x2B63, // chrome - dark S60 green
      0xFFFF, // chrome text
      0xFFE0, // accent
      0xF800, // danger
      0xF7FC, // popup
      0x0000, // popup text
      0x75A3, // popup selected
      0xFFFF  // border
    };
  }

  if (id == ThemeId::AmoledRed) {
    // VQEAF amoled_red palette mapped to the embedded S60 UI:
    // screen=#000000, key=#171717, keyPressed=#38161B,
    // keyBorder=#8A2E3B, keyText=#FFFFFF, subText=#B78E94,
    // shellTop=#141414, shellBorder=#54232B, accent=#FF3D5B.
    return {
      0x0000, // bg          #000000 screen
      0x10A2, // panel       #171717 key
      0x38A3, // selected    #38161B keyPressed
      0xFFFF, // text        #FFFFFF keyText
      0xB472, // dim         #B78E94 subText
      0x10A2, // chrome      #141414 shellTop (same RGB565 bucket)
      0xFFFF, // chrome text #FFFFFF
      0xF9EB, // accent      #FF3D5B
      0xF969, // danger/glow #FF2D4D
      0x0020, // popup       #050505 shellBottom
      0xFFFF, // popup text  #FFFFFF
      0x38A3, // popup selected #38161B
      0x8967  // border      #8A2E3B
    };
  }

  if (id == ThemeId::Black) {
    return {
      0x0000, 0x18C3, 0x39E7, 0xFFFF, 0x9CF3,
      0xD69A, 0x0000, 0x05FF, 0xF800,
      0xD69A, 0x0000, 0x4208, 0xFFFF
    };
  }

  // VQEAF Night default; colors from VQEAF Theme Studio-compatible palette.
  return {
    0x0843, 0x1968, 0x22B0, 0xF7DF, 0xA5DA,
    0x1149, 0xF7DF, 0x5698, 0xF800,
    0x08A4, 0xF7DF, 0x22B0, 0x32AE
  };
}
