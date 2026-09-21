#pragma once

// Legacy-32-Classic E524546 / ESP32-S3-WROOM-1-N16R8

// ST7789 240x320 SPI display
#define BOARD_TFT_BL 39
#define BOARD_TFT_DC 47
#define BOARD_TFT_CS 14
#define BOARD_TFT_SCLK 48
#define BOARD_TFT_MOSI 12
#define BOARD_TFT_RST 3

// Active-low buttons
#define BOARD_KEY_UP 7
#define BOARD_KEY_DOWN 46
#define BOARD_KEY_LEFT 45
#define BOARD_KEY_RIGHT 6
#define BOARD_KEY_MENU 18
#define BOARD_KEY_OPTION 8
#define BOARD_KEY_SELECT 16
#define BOARD_KEY_START 17
#define BOARD_KEY_A 15
#define BOARD_KEY_B 5

// Compatibility aliases for sketches/tools that use the original KEY_* names.
#define KEY_UP BOARD_KEY_UP
#define KEY_DOWN BOARD_KEY_DOWN
#define KEY_LEFT BOARD_KEY_LEFT
#define KEY_RIGHT BOARD_KEY_RIGHT
#define KEY_MENU BOARD_KEY_MENU
#define KEY_OPTION BOARD_KEY_OPTION
#define KEY_SELECT BOARD_KEY_SELECT
#define KEY_START BOARD_KEY_START
#define KEY_A BOARD_KEY_A
#define KEY_B BOARD_KEY_B

// SDMMC 1-bit card slot. CD/DAT3 remains mapped to GPIO10.
#define BOARD_SD_CD_DAT3 10
#define BOARD_SD_DAT3 BOARD_SD_CD_DAT3
#define BOARD_SD_CMD 11
#define BOARD_SD_CLK 13
#define BOARD_SD_DAT0 9
