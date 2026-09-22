#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════
//  LEGACY-32 CLASSIC E524546 - Hardware Configuration
//  ESP32-S3-WROOM-1 (N16R8) | 16MB Flash | 8MB PSRAM
// ═══════════════════════════════════════════════════════════

// ─── TFT ST7789 240x320 ─────────────────────────────────────
#define TFT_LEDK        39   // Backlight (PWM)
#define TFT_DC          47   // Data/Command
#define TFT_CS          14   // Chip Select
#define TFT_SCLK        48   // SPI Clock
#define TFT_MOSI        12   // SPI MOSI (SDA)
#define TFT_RST          3   // Reset
// MISO không dùng cho TFT write-only

// ─── SD Card (SPI) ──────────────────────────────────────────
#define SD_CS           10   // CD/DAT3 - Chip Select
#define SD_MOSI         11   // CMD
#define SD_SCK          13   // CLK
#define SD_MISO          9   // DAT0

// ─── Buttons ────────────────────────────────────────────────
#define KEY_UP           7
#define KEY_DOWN        46
#define KEY_LEFT        45
#define KEY_RIGHT        6
#define KEY_MENU        18
#define KEY_OPTION       8
#define KEY_SELECT      16
#define KEY_START       17
#define KEY_A           15
#define KEY_B            5

// ─── LoRa (SX1276/78 - Optional) ────────────────────────────
#define LORA_SCK        -1   // Set your pins
#define LORA_MISO       -1
#define LORA_MOSI       -1
#define LORA_CS         -1
#define LORA_RST        -1
#define LORA_IRQ        -1

// ─── I2C (Optional peripherals) ──────────────────────────────
#define I2C_SDA         -1
#define I2C_SCL         -1

// ─── Display specs ──────────────────────────────────────────
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   320
#define STATUS_BAR_H     20
#define CONTENT_Y        20   // Below status bar
#define CONTENT_H       300   // 320 - status bar

// ─── TFT_eSPI compatible define ──────────────────────────────
// These match what TFT_eSPI expects in User_Setup.h
// (Set these in User_Setup.h or via build flags)

// ─── System ─────────────────────────────────────────────────
#define BACKLIGHT_PWM_CH   0
#define BACKLIGHT_MAX    255
#define BACKLIGHT_DIM     80
#define BACKLIGHT_OFF      0

// ─── Hardware helper ─────────────────────────────────────────
namespace HW {
  inline void setBacklight(uint8_t brightness) {
    ledcWrite(TFT_LEDK, brightness);
  }
  
  inline void initBacklight() {
    ledcAttach(TFT_LEDK, 5000, 8);
    ledcWrite(TFT_LEDK, BACKLIGHT_MAX);
  }
  
  inline float readBatteryVoltage() {
    // ADC read for battery (connect battery sense to an ADC pin)
    // Placeholder - customize for your hardware
    return 3.7f;
  }
  
  inline int batteryPercent() {
    float v = readBatteryVoltage();
    if (v >= 4.2f) return 100;
    if (v <= 3.0f) return 0;
    return (int)((v - 3.0f) / 1.2f * 100.0f);
  }
}
