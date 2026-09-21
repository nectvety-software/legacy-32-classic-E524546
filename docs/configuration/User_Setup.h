
// User_Setup.h cho ESP32-S3 N16R8 với ST7789 240x320
// Board: ESP32-S3-WROOM-1 (N16R8)
// Flash: 16MB, PSRAM: 8MB (Octal)

#define USER_SETUP_INFO "ESP32-S3 N16R8 ST7789 240x320 Nokia OS"

// ==================== DRIVER ====================
#define ST7789_DRIVER

// ==================== DISPLAY DIMENSIONS ====================
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ==================== PIN DEFINITIONS ====================
// Theo yêu cầu của người dùng:
#define TFT_MOSI 12    // SDA
#define TFT_SCLK 48    // SCL
#define TFT_CS   14
#define TFT_DC   47
#define TFT_RST  3
#define TFT_BL   39    // LEDK - Backlight control

#define TFT_MISO -1    // Không sử dụng

// ==================== SPI CONFIGURATION ====================
#define SPI_FREQUENCY  80000000   // 80MHz cho ESP32-S3
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000

// ==================== FONTS ====================
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ==================== OTHER OPTIONS ====================
#define SUPPORT_TRANSACTIONS
