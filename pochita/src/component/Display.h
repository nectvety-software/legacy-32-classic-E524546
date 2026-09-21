#ifndef POCHITA_DISPLAY_H
#define POCHITA_DISPLAY_H

#include <LovyanGFX.hpp>
#include "BoardPins.h"

// Same ST7789 driver and SPI setup as the working CyberOS reference, adapted
// to the E524546 240x320 panel.
class TFT_eSPI : public lgfx::LGFX_Device {
 private:
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;

 public:
  TFT_eSPI() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.pin_sclk = TFT_SCLK_PIN;
      cfg.pin_mosi = TFT_MOSI_PIN;
      cfg.pin_miso = -1;
      cfg.pin_dc = TFT_DC_PIN;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      auto cfg = panel_.config();
      cfg.pin_cs = TFT_CS_PIN;
      cfg.pin_rst = TFT_RST_PIN;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }

    {
      auto cfg = light_.config();
      cfg.pin_bl = TFT_BL;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 1;
      light_.config(cfg);
      panel_.setLight(&light_);
    }

    setPanel(&panel_);
  }
};

// Compatibility shim for the existing Bluetooth UI sprites.
class TFT_eSprite : public LGFX_Sprite {
 public:
  TFT_eSprite() : LGFX_Sprite() { setPsram(true); }
  explicit TFT_eSprite(LovyanGFX *parent) : LGFX_Sprite(parent) {
    setPsram(true);
  }
};

#endif
