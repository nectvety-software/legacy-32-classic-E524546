#ifndef BOARD_PINS_H
#define BOARD_PINS_H

// legacy-32-classic E524546 / ESP32-S3-WROOM-1 N16R8

// ST7789 LCD
#define TFT_BL 39
#define HARDWARE_RST 3
#define TFT_DC_PIN 47
#define TFT_CS_PIN 14
#define TFT_SCLK_PIN 48
#define TFT_MOSI_PIN 12
#define TFT_RST_PIN 3

// microSD (SPI3/HSPI)
#define SD_CS 10
#define SD_MOSI 11
#define SD_SCLK 13
#define SD_MISO 9

// Physical button positions, active LOW. Direct Symbian mapping per
// docs/system_prompt_phan_cung.md (same repo root):
//   MENU(Home) Up     A(Back)  / Left START(OK) Right / OPTION(Context)
//   Down / B(Delete) + SELECT(Game/T9 toggle).
// Application code uses KEY_* names = roles below.
#define BTN_TOP_LEFT 18       // PCB: KEY_MENU
#define BTN_TOP_CENTER 7      // PCB: KEY_UP
#define BTN_TOP_RIGHT 15      // PCB: KEY_A
#define BTN_MIDDLE_LEFT 45    // PCB: KEY_LEFT
#define BTN_MIDDLE_CENTER 17  // PCB: KEY_START
#define BTN_MIDDLE_RIGHT 6    // PCB: KEY_RIGHT
#define BTN_LOWER_CENTER 46   // PCB: KEY_DOWN
#define BTN_BOTTOM_LEFT 8     // PCB: KEY_OPTION
#define BTN_BOTTOM_RIGHT 5    // PCB: KEY_B
#define BTN_BOTTOM_CENTER 16  // PCB: KEY_SELECT

// Symbian logical layout:
//   Home         Up       Back
//   Left         OK       Right
//   Context      Down     Delete
//                        + Mode toggle (SELECT, bottom center)
#define KEY_MENU BTN_TOP_LEFT
#define KEY_UP BTN_TOP_CENTER
#define KEY_A BTN_TOP_RIGHT
#define KEY_LEFT BTN_MIDDLE_LEFT
#define KEY_START BTN_MIDDLE_CENTER
#define KEY_RIGHT BTN_MIDDLE_RIGHT
#define KEY_DOWN BTN_LOWER_CENTER
#define KEY_OPTION BTN_BOTTOM_LEFT
#define KEY_B BTN_BOTTOM_RIGHT
#define KEY_SELECT BTN_BOTTOM_CENTER

// MAX98357A I2S speaker
#define SPEAKER_DIN 40
#define SPEAKER_BCLK 41
#define SPEAKER_LRCLK 42

// MSM261S4030H0R I2S microphone
#define MIC_WS 36
#define MIC_SD 37
#define MIC_SCK 21

#define STATUS_LED 38

#endif
