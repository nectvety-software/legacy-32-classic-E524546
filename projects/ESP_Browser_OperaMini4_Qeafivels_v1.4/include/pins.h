// Chân kết nối E524546 — sao y DESIGN.txt + docs/system_prompt_phan_cung.md.
// GPIO BẤT KHẢ XÂM PHẠM (luật docs/PROMPT.md §1).
#pragma once
#include <Arduino.h>

// ---- Màn hình TFT 2 inch 240x320 ST7789 (SPI) ----
#define TFT_LEDK   39   // backlight
#define TFT_DC     47   // D/C
#define TFT_CS     14   // CS
#define TFT_SCL    48   // SCL = SCK
#define TFT_SDA    12   // SDA = MOSI
#define TFT_RESET   3   // RESET
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

// ---- 10 nút bấm Symbian 3x3 + SELECT (nối GPIO<->GND, INPUT_PULLUP, nhấn = LOW) ----
#define KEY_MENU    18  // "1"  Home / Main Menu
#define KEY_UP      7   // "2"  Cursor Up
#define KEY_A       15  // "3"  Back / Cancel
#define KEY_LEFT    45  // "4"  Cursor Left
#define KEY_START   17  // "5"  Select / Enter (FIRE/OK)
#define KEY_RIGHT   6   // "6"  Cursor Right
#define KEY_OPTION  8   // "7"  Options / Context
#define KEY_DOWN    46  // "8"  Cursor Down
#define KEY_B       5   // "9"  Delete / Backspace
#define KEY_SELECT  16  // "0"  Giữ >600ms: doi Game/T9

// ---- Thẻ SD (SDMMC 1-bit) ----
#define SD_DAT3_CD  10
#define SD_CMD      11
#define SD_CLK      13
#define SD_DAT0     9
