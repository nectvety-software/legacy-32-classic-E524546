#pragma once

// LCD TFT ST7789, SPI.
constexpr int TFT_BL   = 39;
constexpr int TFT_DC   = 47;
constexpr int TFT_CS   = 14;
constexpr int TFT_SCLK = 48;
constexpr int TFT_MOSI = 12;
constexpr int TFT_RST  = 3;

// Buttons are active LOW and use the ESP32-S3 internal pull-ups.
constexpr int KEY_UP     = 7;
constexpr int KEY_DOWN   = 46;
constexpr int KEY_LEFT   = 45;
constexpr int KEY_RIGHT  = 6;
constexpr int KEY_MENU   = 18;
constexpr int KEY_OPTION = 8;
constexpr int KEY_SELECT = 16;
constexpr int KEY_START  = 17;
constexpr int KEY_A      = 15;
constexpr int KEY_B      = 5;

// SD card in native 1-bit SDMMC mode.
// DAT3/CS (GPIO10) is not used in 1-bit mode.
constexpr int SD_DAT3 = 10;
constexpr int SD_CMD  = 11;
constexpr int SD_CLK  = 13;
constexpr int SD_DAT0 = 9;

