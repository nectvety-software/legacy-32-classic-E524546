#pragma once
#include <Arduino.h>

// ============================================================
// Device: legacy-32-classic E524546
// MCU: ESP32-S3-WROOM-1 (N16R8: 16 MB Flash + 8 MB PSRAM)
// ============================================================
#define DEVICE_NAME "POCHITTA"
#define DEVICE_MODEL "legacy-32-classic E524546"

// ---------- TFT LCD 2.0 inch ST7789, 240x320 ----------
#define SCREEN_W 240
#define SCREEN_H 320
#define DISPLAY_ROTATION 0

#define TFT_BL_PIN    39  // LEDK / Backlight
#define TFT_DC_PIN    47
#define TFT_CS_PIN    14
#define TFT_SCLK_PIN   4
#define TFT_MOSI_PIN  12
#define TFT_RST_PIN    3

#ifndef TFT_BL
#define TFT_BL TFT_BL_PIN
#endif

// Backlight PWM
#define TFT_BL_PWM_CHANNEL 0
#define TFT_BL_PWM_FREQ    5000
#define TFT_BL_PWM_BITS    8

// ---------- 10 active-low buttons ----------
#define PIN_KEY_UP       7
#define PIN_KEY_DOWN    46
#define PIN_KEY_LEFT    45
#define PIN_KEY_RIGHT    6
#define PIN_KEY_MENU    18
#define PIN_KEY_OPTION   8
#define PIN_KEY_SELECT  16
#define PIN_KEY_START   17
#define PIN_KEY_A       15
#define PIN_KEY_B        5

// Button behavior (docs/system_prompt_phan_cung.md):
// START(center) = OK/confirm, A = Back/Cancel, MENU = Home,
// B = Delete, OPTION = Context, SELECT = Game/T9 toggle (hold >600ms).
#define KEY_ACTIVE_LEVEL LOW
#define KEY_INPUT_MODE INPUT_PULLUP
#define KEY_DEBOUNCE_MS 25
#define KEY_REPEAT_GUARD_MS 145

// ---------- SD card: native SD_MMC 1-bit mode ----------
// In 1-bit SD_MMC mode only CLK, CMD and DAT0 are required.
// DAT3/CD is declared for board reference and is not driven as SPI CS.
#define USE_SD_CARD 1
#define SD_MMC_CLK_PIN   13
#define SD_MMC_CMD_PIN   11
#define SD_MMC_D0_PIN     9
#define SD_MMC_D3_CD_PIN 10
#define SD_MMC_MOUNT_POINT "/sdcard"
#define SD_MMC_ONE_BIT_MODE true
#define SD_MMC_FORMAT_IF_FAIL false

// ---------- Optional peripherals ----------
#define USE_LORA 0
#define LORA_CS_PIN   -1
#define LORA_DIO0_PIN -1
#define LORA_RST_PIN  -1
#define LORA_DIO1_PIN -1
#define LORA_FREQ_MHZ 433.0

#define USE_IR 0
#define IR_TX_PIN -1
#define IR_RX_PIN -1
