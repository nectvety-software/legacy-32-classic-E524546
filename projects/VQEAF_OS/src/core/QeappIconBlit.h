#pragma once
#include <stdint.h>
#include <TFT_eSPI.h>

// QEAPP/2 icon on SD: 32x32 RGB565 little-endian. ESP32-S3 is little-endian.
// Pixels read into uint16_t need TFT_eSPI's swapBytes(true) for SPI output.
// Preserve any surrounding renderer's swap state (JPEG/PNG set their own).
namespace QeappIconBlit {
inline void draw(TFT_eSPI &lcd, int16_t x, int16_t y,
                 const uint16_t pixels[1024]) {
  if (!pixels) return;
  const bool previous = lcd.getSwapBytes();
  lcd.setSwapBytes(true);
  lcd.pushImage(x, y, 32, 32, pixels);
  lcd.setSwapBytes(previous);
}
} // namespace QeappIconBlit
