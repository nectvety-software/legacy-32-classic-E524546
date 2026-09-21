// Chân kết nối — KHỚP system_prompt_phan_cung.md (Symbian keypad 3x3 + SELECT).
#pragma once
#include <Arduino.h>

// ---- Màn hình TFT 2 inch 240x320 ST7789 (SPI) ----
// VCC - VCC | GND - GND (cấp nguồn ngoài, chung GND)
#define TFT_LEDK   39   // backlight
#define TFT_DC     47   // D/C
#define TFT_CS     14   // CS
#define TFT_SCL    48   // SCL = SCK
#define TFT_SDA    12   // SDA = MOSI
#define TFT_RESET   3   // RESET
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

// ---- 10 nút bấm Symbian (nối GPIO <-> GND, INPUT_PULLUP, nhấn = LOW) ----
// | Vi tri      | Phim       | GPIO | So (T9) | Vai tro OS              |
// | Top Left    | KEY_MENU   | 18   | 1       | Main Menu / Home        |
// | Top Center  | KEY_UP     | 7    | 2       | Cursor Up               |
// | Top Right   | KEY_A      | 15   | 3       | Back / Cancel           |
// | Mid Left    | KEY_LEFT   | 45   | 4       | Cursor Left             |
// | Center      | KEY_START  | 17   | 5       | Select / Enter (FIRE/OK)|
// | Mid Right   | KEY_RIGHT  | 6    | 6       | Cursor Right            |
// | Bot Left    | KEY_OPTION | 8    | 7 (* khi giu) | Options / Context  |
// | Mid Bot     | KEY_DOWN   | 46   | 8       | Cursor Down             |
// | Bot Right   | KEY_B      | 5    | 9 (# khi giu) | Delete / Backspace  |
// | Bottom      | KEY_SELECT | 16   | 0       | Giu >600ms: doi Game/T9 |
#define KEY_MENU    18
#define KEY_UP      7
#define KEY_A       15
#define KEY_LEFT    45
#define KEY_START   17
#define KEY_RIGHT   6
#define KEY_OPTION  8
#define KEY_DOWN    46
#define KEY_B       5
#define KEY_SELECT  16

// ---- Thẻ SD (SPI / SDMMC 1-bit của ESP32-S3) ----
// VCC - VDD | GND - GND
#define SD_DAT3_CD  10  // CD/DAT3 (CS)
#define SD_CMD      11  // CMD (MOSI)
#define SD_CLK      13  // CLK (SCLK)
#define SD_DAT0     9   // DAT0 (MISO)
