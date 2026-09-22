// ═══════════════════════════════════════════════════════════
//  TFT_eSPI User Setup for LEGACY-32 CLASSIC E524546
//  Display: ST7789 240x320 TFT
//  MCU: ESP32-S3-WROOM-1 (N16R8)
//
//  IMPORTANT: Copy this file to:
//  Arduino/libraries/TFT_eSPI/User_Setup.h
//  (Replace the existing User_Setup.h)
// ═══════════════════════════════════════════════════════════

// ─── Driver selection ─────────────────────────────────────
#define ST7789_DRIVER   // ST7789 240x320

// ─── Screen size ──────────────────────────────────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ─── Pin definitions ──────────────────────────────────────
#define TFT_MOSI    12   // SDA
#define TFT_SCLK    48   // SCL
#define TFT_CS      14   // CS
#define TFT_DC      47   // D/C
#define TFT_RST      3   // RESET
// No MISO needed (write-only display)

// ─── Backlight ────────────────────────────────────────────
// Controlled via PWM in code (pin 39)
// #define TFT_BL   39   // Optional - we control manually

// ─── SPI frequency ────────────────────────────────────────
#define SPI_FREQUENCY       40000000   // 40MHz for ST7789
#define SPI_READ_FREQUENCY   6000000
#define SPI_TOUCH_FREQUENCY  2500000

// ─── SPI port ─────────────────────────────────────────────
#define USE_HSPI_PORT

// ─── Color order ──────────────────────────────────────────
// Uncomment if colors are inverted:
// #define TFT_RGB_ORDER TFT_BGR

// ─── Display inversion ────────────────────────────────────
// ST7789 usually needs inversion enabled:
#define TFT_INVERSION_ON

// ─── Fonts ────────────────────────────────────────────────
#define LOAD_GLCD    // Font 1
#define LOAD_FONT2   // Font 2
#define LOAD_FONT4   // Font 4
#define LOAD_FONT6   // Font 6
#define LOAD_FONT7   // Font 7
#define LOAD_FONT8   // Font 8
#define LOAD_GFXFF   // FreeFonts

// ─── Smooth fonts ─────────────────────────────────────────
#define SMOOTH_FONT

// ─── DMA transfers ────────────────────────────────────────
//#define ESP32_DMA      // Enable if using DMA (optional)

// ─── Notes ────────────────────────────────────────────────
// This file must be in TFT_eSPI library folder
// Board: "ESP32S3 Dev Module"
// Flash: 16MB QIO 80MHz
// PSRAM: OPI PSRAM
// Partition: "Huge APP (3MB No OTA/1MB SPIFFS)"
// or "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
// USB CDC: Enabled
// Core Debug Level: None (for release)
