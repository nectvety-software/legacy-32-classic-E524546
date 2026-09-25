#pragma once
#include <Arduino.h>

// legacy-32-classic-E524546 board mapping.
// v0.4 follows the user's physical enclosure: native portrait 240x320.
namespace Board {
constexpr int TFT_LEDK_PIN = 39;
constexpr int TFT_DC_PIN   = 47;
constexpr int TFT_CS_PIN   = 14;
constexpr int TFT_SCL_PIN  = 48;
constexpr int TFT_SDA_PIN  = 12;
constexpr int TFT_RST_PIN  = 3;

constexpr int KEY_MENU   = 18;
constexpr int KEY_UP     = 7;
constexpr int KEY_A      = 15;
constexpr int KEY_LEFT   = 45;
constexpr int KEY_START  = 17;
constexpr int KEY_RIGHT  = 6;
constexpr int KEY_OPTION = 8;
constexpr int KEY_DOWN   = 46;
constexpr int KEY_B      = 5;
constexpr int KEY_SELECT = 16;

constexpr int SD_D3  = 10;
constexpr int SD_CMD = 11;
constexpr int SD_CLK = 13;
constexpr int SD_D0  = 9;

// ST7789 portrait: 240x320.
constexpr uint8_t TFT_ROTATION = 0;
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;

// Optional external I2S audio (MAX98357A / PCM5102-style DAC).
// These GPIOs are free in the documented E524546 pin map and avoid USB GPIO19/20,
// flash/PSRAM GPIO26..37, TFT, keypad and SD pins.
// Wiring default: GPIO4=BCLK, GPIO1=LRCLK/WS, GPIO2=DIN.
#ifndef SYMBIAN_AUDIO_ENABLED
#define SYMBIAN_AUDIO_ENABLED 1
#endif
#ifndef SYMBIAN_AUDIO_BCLK
#define SYMBIAN_AUDIO_BCLK 4
#endif
#ifndef SYMBIAN_AUDIO_WS
#define SYMBIAN_AUDIO_WS 1
#endif
#ifndef SYMBIAN_AUDIO_DOUT
#define SYMBIAN_AUDIO_DOUT 2
#endif
constexpr int AUDIO_BCLK = SYMBIAN_AUDIO_BCLK;
constexpr int AUDIO_WS   = SYMBIAN_AUDIO_WS;
constexpr int AUDIO_DOUT = SYMBIAN_AUDIO_DOUT;
}
